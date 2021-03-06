package com.yahoo.vespa.hosted.node.verification.commons.report;

import com.fasterxml.jackson.databind.ObjectMapper;

import com.yahoo.vespa.hosted.node.verification.commons.noderepo.NodeRepoInfoRetriever;
import com.yahoo.vespa.hosted.node.verification.commons.noderepo.NodeRepoJsonModel;

import java.io.IOException;
import java.net.URL;
import java.util.ArrayList;
import java.util.logging.Level;
import java.util.logging.Logger;

public class ReportSender {

    private static final Logger logger = Logger.getLogger(ReportSender.class.getName());

    private static void printHardwareDivergenceReport(HardwareDivergenceReport hardwareDivergenceReport) throws IOException {
        ObjectMapper om = new ObjectMapper();
        String report;
        if (hardwareDivergenceReport.isHardwareDivergenceReportEmpty()){
            report = "null";
        }
        else {
            report = om.writeValueAsString(hardwareDivergenceReport);
        }
        System.out.print(report);
    }

    public static void reportBenchmarkResults(BenchmarkReport benchmarkReport, ArrayList<URL> nodeInfoUrls) throws IOException {
        HardwareDivergenceReport hardwareDivergenceReport = generateHardwareDivergenceReport(nodeInfoUrls);
        hardwareDivergenceReport.setBenchmarkReport(benchmarkReport);
        printHardwareDivergenceReport(hardwareDivergenceReport);
    }

    public static void reportSpecVerificationResults(SpecVerificationReport specVerificationReport, ArrayList<URL> nodeInfoUrls) throws IOException {
        HardwareDivergenceReport hardwareDivergenceReport = generateHardwareDivergenceReport(nodeInfoUrls);
        hardwareDivergenceReport.setSpecVerificationReport(specVerificationReport);
        printHardwareDivergenceReport(hardwareDivergenceReport);
    }

    private static HardwareDivergenceReport generateHardwareDivergenceReport(ArrayList<URL> nodeInfoUrls) throws IOException {
        NodeRepoJsonModel nodeRepoJsonModel = NodeRepoInfoRetriever.retrieve(nodeInfoUrls);
        ObjectMapper om = new ObjectMapper();
        if (nodeRepoJsonModel.getHardwareDivergence() == null || nodeRepoJsonModel.getHardwareDivergence().equals("null")) {
            return new HardwareDivergenceReport();
        }
        try {
            HardwareDivergenceReport hardwareDivergenceReport = om.readValue(nodeRepoJsonModel.getHardwareDivergence(), HardwareDivergenceReport.class);
            return hardwareDivergenceReport;
        }
        catch (IOException e){
            logger.log(Level.WARNING, "Failed to parse hardware divergence report from node repo. Report:\n" + nodeRepoJsonModel.getHardwareDivergence(), e.getMessage());
            return new HardwareDivergenceReport();
        }
    }
}
