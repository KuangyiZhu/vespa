// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "fieldinverter.h"
#include "ordereddocumentinserter.h"
#include <vespa/document/datatype/urldatatype.h>
#include <vespa/searchlib/util/url.h>
#include <stdexcept>
#include <vespa/vespalib/text/utf8.h>
#include <vespa/vespalib/text/lowercase.h>
#include <vespa/searchlib/common/sort.h>
#include <vespa/searchlib/bitcompression/compression.h>
#include <vespa/searchlib/bitcompression/posocccompression.h>
#include <vespa/document/annotation/annotation.h>
#include <vespa/document/annotation/span.h>
#include <vespa/document/annotation/spanlist.h>
#include <vespa/document/annotation/alternatespanlist.h>
#include <vespa/document/annotation/spantree.h>
#include <vespa/document/annotation/spantreevisitor.h>

namespace search {

namespace memoryindex {

using document::AlternateSpanList;
using document::Annotation;
using document::AnnotationType;
using document::ArrayFieldValue;
using document::DataType;
using document::Document;
using document::DocumentType;
using document::Field;
using document::FieldValue;
using document::IntFieldValue;
using document::SimpleSpanList;
using document::Span;
using document::SpanList;
using document::SpanNode;
using document::SpanTree;
using document::SpanTreeVisitor;
using document::StringFieldValue;
using document::StructFieldValue;
using document::WeightedSetFieldValue;
using index::DocIdAndPosOccFeatures;
using index::Schema;
using search::index::schema::CollectionType;
using search::util::URL;
using vespalib::make_string;

namespace documentinverterkludge {

namespace linguistics {

const vespalib::string SPANTREE_NAME("linguistics");

}

}

using namespace documentinverterkludge;

namespace
{

class SpanFinder : public SpanTreeVisitor
{
public:
    int32_t begin_pos;
    int32_t end_pos;

    SpanFinder() : begin_pos(0x7fffffff), end_pos(-1) {}
    Span span() { return Span(begin_pos, end_pos - begin_pos); }

    void visit(const Span &node) override {
        begin_pos = std::min(begin_pos, node.from());
        end_pos = std::max(end_pos, node.from() + node.length());
    }
    void visit(const SpanList &node) override {
        for (const auto & span_ : node) {
            const_cast<SpanNode *>(span_)->accept(*this);
        }
    }
    void visit(const SimpleSpanList &node) override {
        for (const auto & span_ : node) {
            const_cast<Span &>(span_).accept(*this);
        }
    }
    void visit(const AlternateSpanList &node) override {
        for (size_t i = 0; i < node.getNumSubtrees(); ++i) {
            visit(node.getSubtree(i));
        }
    }
};

Span
getSpan(const SpanNode &span_node)
{
    SpanFinder finder;
    // The SpanNode will not be changed.
    const_cast<SpanNode &>(span_node).accept(finder);
    return finder.span();
}

}

void
FieldInverter::processAnnotations(const StringFieldValue &value)
{
    _terms.clear();
    StringFieldValue::SpanTrees spanTrees = value.getSpanTrees();
    const SpanTree *tree = StringFieldValue::findTree(spanTrees, linguistics::SPANTREE_NAME);
    if (tree == NULL) {
        /* This is wrong unless field is exact match */
        const vespalib::string &text = value.getValue();
        if (text.empty())
            return;
        uint32_t wordRef = saveWord(text);
        if (wordRef != 0u) {
            add(wordRef);
            stepWordPos();
        }
        return;
    }
    const vespalib::string &text = value.getValue();
    for (const Annotation & annotation : *tree) {
        const SpanNode *span = annotation.getSpanNode();
        if ((span != nullptr) && annotation.valid() &&
            (annotation.getType() == *AnnotationType::TERM))
        {
            Span sp = getSpan(*span);
            if (sp.length() != 0) {
                _terms.push_back(std::make_pair(sp,
                                                annotation.getFieldValue()));
            }
        }
    }
    std::sort(_terms.begin(), _terms.end());
    SpanTermVector::const_iterator it  = _terms.begin();
    SpanTermVector::const_iterator ite = _terms.end();
    uint32_t wordRef;
    bool mustStep = false;
    for (; it != ite; ) {
        SpanTermVector::const_iterator it_begin = it;
        for (; it != ite && it->first == it_begin->first; ++it) {
            if (it->second) {  // it->second is a const FieldValue *.
                wordRef = saveWord(*it->second);
            } else {
                const Span &iSpan = it->first;
                assert(iSpan.from() >= 0);
                assert(iSpan.length() > 0);
                wordRef = saveWord(vespalib::stringref(&text[iSpan.from()],
                                                       iSpan.length()));
            }
            if (wordRef != 0u) {
                add(wordRef);
                mustStep = true;
            }
        }
        if (mustStep) {
            stepWordPos();
            mustStep = false;
        }
    }
}


void
FieldInverter::reset()
{
    _words.clear();
    _elems.clear();
    _positions.clear();
    _wordRefs.resize(1);
    _pendingDocs.clear();
    _abortedDocs.clear();
    _removeDocs.clear();
    _oldPosSize = 0u;
}

struct WordRefRadix {
    uint32_t operator () (const uint64_t v) { return v >> 32; }
};

void
FieldInverter::sortWords()
{
    assert(_wordRefs.size() > 1);

    // Make a dictionary for words.
    { // Use radix sort based on first four bytes of word, before finalizing with std::sort.
        vespalib::Array<uint64_t> firstFourBytes(_wordRefs.size());
        for (size_t i(1); i < _wordRefs.size(); i++) {
            uint64_t firstFour = ntohl(*reinterpret_cast<const uint32_t *>(getWordFromRef(_wordRefs[i])));
            firstFourBytes[i] = (firstFour << 32) | _wordRefs[i];
        }
        ShiftBasedRadixSorter<uint64_t, WordRefRadix, CompareWordRef, 24, true>::
           radix_sort(WordRefRadix(), CompareWordRef(_words), &firstFourBytes[1], firstFourBytes.size()-1, 16);
        for (size_t i(1); i < firstFourBytes.size(); i++) {
            _wordRefs[i] = firstFourBytes[i] & 0xffffffffl;
        }
    }
    // Populate word numbers in word buffer and mapping from
    // word numbers to word reference.
    // TODO: shrink word buffer to only contain unique words
    std::vector<uint32_t>::const_iterator w(_wordRefs.begin() + 1);
    std::vector<uint32_t>::const_iterator we(_wordRefs.end());
    uint32_t wordNum = 1;	// First valid word number
    const char *lastWord = getWordFromRef(*w);
    updateWordNum(*w, wordNum);
    for (++w; w != we; ++w) {
        const char *word = getWordFromRef(*w);
        int cmpres = strcmp(lastWord, word);
        assert(cmpres <= 0);
        if (cmpres < 0) {
            ++wordNum;
            _wordRefs[wordNum] = *w;
            lastWord = word;
        }
        updateWordNum(*w, wordNum);
    }
    assert(_wordRefs.size() >= wordNum + 1);
    _wordRefs.resize(wordNum + 1);
    // Replace initial word reference by word number.
    for (auto &p : _positions) {
        p._wordNum = getWordNum(p._wordNum);
    }
}


void
FieldInverter::startElement(int32_t weight)
{
    _elems.push_back(ElemInfo(weight));	// Fill in length later
}


void
FieldInverter::endElement()
{
    _elems.back().setLen(_wpos);
    _wpos = 0;
    ++_elem;
}

uint32_t
FieldInverter::saveWord(const vespalib::stringref word)
{
    const size_t wordsSize = _words.size();
    // assert((wordsSize & 3) == 0); // Check alignment
    size_t len = word.size();
    if (len == 0)
        return 0u;

    const size_t fullyPaddedSize = (wordsSize + 4 + len + 1 + 3) & ~3;
    _words.reserve(vespalib::roundUp2inN(fullyPaddedSize));
    _words.resize(fullyPaddedSize);

    char * buf = &_words[0] + wordsSize;
    memset(buf, 0, 4);
    memcpy(buf + 4, word.c_str(), len);
    uint32_t *lastWord = reinterpret_cast<uint32_t *>(buf + 4 + (len & ~0x3));
    *lastWord &=  (0xffffff >> ((3 - (len & 3)) << 3)); //only on little endian machiness !!

    uint32_t wordRef = (wordsSize + 4) >> 2;
    // assert(wordRef != 0);
    _wordRefs.push_back(wordRef);
    return wordRef;
}


uint32_t
FieldInverter::saveWord(const document::FieldValue &fv)
{
    assert(fv.getClass().id() == StringFieldValue::classId);
    typedef std::pair<const char*, size_t> RawRef;
    RawRef sRef = fv.getAsRaw();
    return saveWord(vespalib::stringref(sRef.first, sRef.second));
}


void
FieldInverter::remove(const vespalib::stringref word, uint32_t docId)
{
    uint32_t wordRef = saveWord(word);
    assert(wordRef != 0);
    _positions.emplace_back(wordRef, docId);
}


void
FieldInverter::processNormalDocTextField(const StringFieldValue &field)
{
    startElement(1);
    processAnnotations(field);
    endElement();
}


void
FieldInverter::processNormalDocArrayTextField(const ArrayFieldValue &field)
{
    uint32_t el = 0;
    uint32_t ele = field.size();
    for (;el < ele; ++el) {
        const FieldValue &elfv = field[el];
        assert(elfv.getClass().id() == StringFieldValue::classId);
        const StringFieldValue &element =
            static_cast<const StringFieldValue &>(elfv);
        startElement(1);
        processAnnotations(element);
        endElement();
    }
}


void
FieldInverter::processNormalDocWeightedSetTextField(const WeightedSetFieldValue &field)
{
    for (const auto & el : field) {
        const FieldValue &key = *el.first;
        const FieldValue &xweight = *el.second;
        assert(key.getClass().id() == StringFieldValue::classId);
        assert(xweight.getClass().id() == IntFieldValue::classId);
        const StringFieldValue &element = static_cast<const StringFieldValue &>(key);
        int32_t weight = xweight.getAsInt();
        startElement(weight);
        processAnnotations(element);
        endElement();
    }
}


FieldInverter::FieldInverter(const Schema &schema, uint32_t fieldId)
    : _fieldId(fieldId),
      _elem(0u),
      _wpos(0u),
      _docId(0),
      _oldPosSize(0),
      _schema(schema),
      _words(),
      _elems(),
      _positions(),
      _features(),
      _elementWordRefs(),
      _wordRefs(1),
      _terms(),
      _abortedDocs(),
      _pendingDocs(),
      _removeDocs()
{
}


void
FieldInverter::abortPendingDoc(uint32_t docId)
{
    auto itr = _pendingDocs.find(docId);
    if (itr != _pendingDocs.end()) {
        if (itr->second.getLen() != 0) {
            _abortedDocs.push_back(itr->second);
        }
        _pendingDocs.erase(itr);
    }
}


void
FieldInverter::moveNotAbortedDocs(uint32_t &dstIdx,
                                  uint32_t srcIdx,
                                  uint32_t nextTrimIdx)
{
    assert(nextTrimIdx >= srcIdx);
    uint32_t size = nextTrimIdx - srcIdx;
    if (size == 0)
        return;
    assert(dstIdx < srcIdx);
    assert(srcIdx < _positions.size());
    assert(srcIdx + size <= _positions.size());
    PosInfo *dst = &_positions[dstIdx];
    const PosInfo *src = &_positions[srcIdx];
    const PosInfo *srce = src + size;
    while (src != srce) {
        *dst = *src;
        ++dst;
        ++src;
    }
    dstIdx += size;
}


void
FieldInverter::trimAbortedDocs()
{
    if (_abortedDocs.empty()) {
        return;
    }
    std::sort(_abortedDocs.begin(), _abortedDocs.end());
    auto itrEnd = _abortedDocs.end();
    auto itr = _abortedDocs.begin();
    uint32_t dstIdx = itr->getStart();
    uint32_t srcIdx = itr->getStart() + itr->getLen();
    ++itr;
    while (itr != itrEnd) {
        moveNotAbortedDocs(dstIdx, srcIdx, itr->getStart());
        srcIdx = itr->getStart() + itr->getLen();
        ++itr;
    }
    moveNotAbortedDocs(dstIdx, srcIdx, _positions.size());
    _positions.resize(dstIdx);
    _abortedDocs.clear();
}


void
FieldInverter::invertField(uint32_t docId, const FieldValue::UP &val)
{
    startDoc(docId);
    if (val) {
        invertNormalDocTextField(*val);
    }
    endDoc();
}


void
FieldInverter::invertNormalDocTextField(const FieldValue &val)
{
    const vespalib::Identifiable::RuntimeClass & cInfo(val.getClass());
    const Schema::IndexField &field = _schema.getIndexField(_fieldId);
    switch (field.getCollectionType()) {
    case CollectionType::SINGLE:
        if (cInfo.id() == StringFieldValue::classId) {
            processNormalDocTextField(static_cast<const StringFieldValue &>(val));
        } else {
            throw std::runtime_error(make_string("Expected DataType::STRING, got '%s'", val.getDataType()->getName().c_str()));
        }
        break;
    case CollectionType::WEIGHTEDSET:
        if (cInfo.id() == WeightedSetFieldValue::classId) {
            const WeightedSetFieldValue &wset = static_cast<const WeightedSetFieldValue &>(val);
            if (wset.getNestedType() == *DataType::STRING) {
                processNormalDocWeightedSetTextField(wset);
            } else {
                throw std::runtime_error(make_string("Expected DataType::STRING, got '%s'", wset.getNestedType().getName().c_str()));
            }
        } else {
            throw std::runtime_error(make_string("Expected weighted set, got '%s'", cInfo.name()));
        }
        break;
    case CollectionType::ARRAY:
        if (cInfo.id() == ArrayFieldValue::classId) {
            const ArrayFieldValue &arr = static_cast<const ArrayFieldValue&>(val);
            if (arr.getNestedType() == *DataType::STRING) {
                processNormalDocArrayTextField(arr);
            } else {
                throw std::runtime_error(make_string("Expected DataType::STRING, got '%s'", arr.getNestedType().getName().c_str()));
            }
        } else {
            throw std::runtime_error(make_string("Expected Array, got '%s'", cInfo.name()));
        }
        break;
    default:
        break;
    }
}


namespace {

struct FullRadix {
    uint64_t operator () (const FieldInverter::PosInfo & p) const {
        return (static_cast<uint64_t>(p._wordNum) << 32) |
            p._docId;
    }
};

}


void
FieldInverter::applyRemoves(DocumentRemover &remover)
{
    for (auto docId : _removeDocs) {
        remover.remove(docId, *this);
    }
    _removeDocs.clear();
}


void
FieldInverter::pushDocuments(IOrderedDocumentInserter &inserter)
{
    trimAbortedDocs();

    if (_positions.empty()) {
        reset();
        return;				// All documents with words aborted
    }

    sortWords();

    // Sort for terms.
    ShiftBasedRadixSorter<PosInfo, FullRadix, std::less<PosInfo>, 56, true>::
        radix_sort(FullRadix(), std::less<PosInfo>(), &_positions[0], _positions.size(), 16);

    constexpr uint32_t NO_ELEMENT_ID = std::numeric_limits<uint32_t>::max();
    constexpr uint32_t NO_WORD_POS = std::numeric_limits<uint32_t>::max();
    uint32_t lastWordNum = 0;
    uint32_t lastElemId = 0;
    uint32_t lastWordPos = 0;
    uint32_t numWordIds = _wordRefs.size() - 1;
    uint32_t lastDocId = 0;
    vespalib::stringref word;
    bool emptyFeatures = true;

    inserter.rewind();

    for (auto &i : _positions) {
        assert(i._wordNum <= numWordIds);
        (void) numWordIds;
        if (lastWordNum != i._wordNum || lastDocId != i._docId) {
            if (!emptyFeatures) {
                inserter.add(lastDocId, _features);
                emptyFeatures = true;
            }
            if (lastWordNum != i._wordNum) {
                lastWordNum = i._wordNum;
                word = getWordFromNum(lastWordNum);
                inserter.setNextWord(word);
            }
            lastDocId = i._docId;
            if (i.removed()) {
                inserter.remove(lastDocId);
                continue;
            }
        }
        if (emptyFeatures) {
            if (!i.removed()) {
                emptyFeatures = false;
                _features.clear(lastDocId);
                lastElemId = NO_ELEMENT_ID;
                lastWordPos = NO_WORD_POS;
            } else {
                continue; // ignore dup remove
            }
        } else {
            // removes must come before non-removes
            assert(!i.removed());
        }
        const ElemInfo &elem = _elems[i._elemRef];
        if (i._wordPos != lastWordPos || i._elemId != lastElemId) {
            _features.addNextOcc(i._elemId, i._wordPos,
                                 elem._weight, elem._len);
            lastElemId = i._elemId;
            lastWordPos = i._wordPos;
        } else {
            // silently ignore duplicate annotations
        }
    }

    if (!emptyFeatures) {
        inserter.add(lastDocId, _features);
    }
    inserter.flush();
    reset();
}


} // namespace memoryindex

} // namespace search

