// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.hosted.dockerapi;

import java.io.File;
import java.net.InetAddress;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;

/**
 * API to simplify the com.github.dockerjava API for clients,
 * and to avoid OSGi exporting those classes.
 */
public interface Docker {
    interface CreateContainerCommand {
        CreateContainerCommand withLabel(String name, String value);
        CreateContainerCommand withEnvironment(String name, String value);
        CreateContainerCommand withVolume(String path, String volumePath);
        CreateContainerCommand withMemoryInMb(long megaBytes);
        CreateContainerCommand withCpuShares(int shares);
        CreateContainerCommand withNetworkMode(String mode);
        CreateContainerCommand withIpAddress(InetAddress address);
        CreateContainerCommand withUlimit(String name, int softLimit, int hardLimit);
        CreateContainerCommand withEntrypoint(String... entrypoint);
        CreateContainerCommand withManagedBy(String manager);
        CreateContainerCommand withAddCapability(String capabilityName);
        CreateContainerCommand withDropCapability(String capabilityName);

        void create();
    }

    CreateContainerCommand createContainerCommand(
            DockerImage dockerImage,
            ContainerName containerName,
            String hostName);

    interface ContainerStats {
        Map<String, Object> getNetworks();
        Map<String, Object> getCpuStats();
        Map<String, Object> getMemoryStats();
        Map<String, Object> getBlkioStats();
    }

    Optional<ContainerStats> getContainerStats(ContainerName containerName);
    
    void startContainer(ContainerName containerName);

    void stopContainer(ContainerName containerName);

    void deleteContainer(ContainerName containerName);

    void connectContainerToNetwork(ContainerName containerName, String networkName);

    void copyArchiveToContainer(String sourcePath, ContainerName destinationContainer, String destinationPath);

    List<Container> getAllContainersManagedBy(String manager);

    List<ContainerName> listAllContainersManagedBy(String manager);

    Optional<Container> getContainer(ContainerName containerName);

    CompletableFuture<DockerImage> pullImageAsync(DockerImage image);

    boolean imageIsDownloaded(DockerImage image);

    void deleteImage(DockerImage dockerImage);

    void buildImage(File dockerfile, DockerImage dockerImage);

    /**
     * Deletes the local images that are currently not in use by any container and not recently used.
     */
    void deleteUnusedDockerImages();

    /**
     * Execute a command in docker container as "yahoo". Will block until the command is finished.
     *
     * @param containerName The name of the container
     * @param command The command with arguments to run.
     *
     * @return exitcodes, stdout and stderr in the ProcessResult
     */
    ProcessResult executeInContainer(ContainerName containerName, String... command);

    /**
     * Execute a command in docker container as "root". Will block until the command is finished.
     *
     * @param containerName The name of the container
     * @param command The command with arguments to run.
     *
     * @return exitcodes, stdout and stderr in the ProcessResult
     */
    ProcessResult executeInContainerAsRoot(ContainerName containerName, String... command);

    /**
     * Execute a command in docker container as "root"
     * The timeout will not kill the process spawned.
     *
     * @param containerName The name of the container
     * @param timeoutSeconds Timeout for the process to finish in seconds or without timeout if empty
     * @param command The command with arguments to run.
     *
     * @return exitcodes, stdout and stderr in the ProcessResult
     */
    ProcessResult executeInContainerAsRoot(ContainerName containerName, Long timeoutSeconds, String... command);


}
