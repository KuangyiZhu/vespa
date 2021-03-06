// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
// Copyright (C) 2006 Yahoo

#pragma once

// indirectly tested by exception test.

/**
 * @def VESPA_STRINGIZE(str)
 * @brief convert code to string
 * @param str text that will be converted to a string
 * @return the replacement text from expansion of this macro will be a "" quoted C string literal.
 **/
#define VESPA_STRINGIZE(str) #str

/**
 * @def VESPA_STRLOC
 * @brief generate a string pointing to line in source code
 *
 * this macro can be used in detailed error messages that need
 * information about where in the source code they were generated,
 * typically in exceptions.
 * @return a std::string containing the function, source file, and line number
 **/
#define VESPA_STRLOC vespalib::make_string("%s in %s:%d",__func__,__FILE__,__LINE__)

