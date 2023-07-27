#include <iomanip>
#include <iostream>
#include <mutex>
#include <regex>
#include <vector>
#include <getopt.h>
#include <string>
#include <thread>
#include <cstdio>
#include <chrono>
#include <errno.h>
#include <csignal>
#include <sstream>
#include <unistd.h>
#include <fstream>
#include <limits.h>
#include <sys/inotify.h>
#include <grpcpp/grpcpp.h>
#include <utime.h>

#include "src/dfs-utils.h"
#include "src/dfslibx-clientnode-p2.h"
#include "dfslib-shared-p2.h"
#include "dfslib-clientnode-p2.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientWriter;
using grpc::Status;
using grpc::StatusCode;

using dfs_service::File;
using dfs_service::StoreRes;
using dfs_service::FetchRequest;
using dfs_service::DeleteRequest;
using dfs_service::DeleteRes;
using dfs_service::ListRequest;
using dfs_service::ListRes;
using dfs_service::StatusRequest;
using dfs_service::StatusRes;
using dfs_service::FileInfo;
using dfs_service::WriteLockRequest;
using dfs_service::WriteLockRes;
using dfs_service::CallbackListRequest;
using dfs_service::CallbackListRes;

extern dfs_log_level_e DFS_LOG_LEVEL;

//
// STUDENT INSTRUCTION:
//
// Change these "using" aliases to the specific
// message types you are using to indicate
// a file request and a listing of files from the server.
//
using FileRequestType = CallbackListRequest;
using FileListResponseType = CallbackListRes;

DFSClientNodeP2::DFSClientNodeP2() : DFSClientNode() {}
DFSClientNodeP2::~DFSClientNodeP2() {}



grpc::StatusCode DFSClientNodeP2::RequestWriteAccess(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to obtain a write lock here when trying to store a file.
    // This method should request a write lock for the given file at the server,
    // so that the current client becomes the sole creator/writer. If the server
    // responds with a RESOURCE_EXHAUSTED response, the client should cancel
    // the current file storage
    //
    // The StatusCode response should be:
    //
    // OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //
    WriteLockRequest request;
    WriteLockRes response;
    ClientContext context;

    // set timeout
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));

    request.set_clientid(ClientId());
    request.set_name(filename);
    Status status = service_stub->WriteLock(&context, request, &response);

    if (status.ok()) {
        return StatusCode::OK;
    } else if (status.error_code() == 4) {
        return StatusCode::DEADLINE_EXCEEDED;
    } else if (status.error_code() == 8) {
        return StatusCode::RESOURCE_EXHAUSTED;
    } else {
        std::cout << "Error: " << status.error_message() << std::endl;
        return StatusCode::CANCELLED;
    }
}


grpc::StatusCode DFSClientNodeP2::Store(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to store a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation. However, you will
    // need to adjust this method to recognize when a file trying to be
    // stored is the same on the server (i.e. the ALREADY_EXISTS gRPC response).
    //
    // You will also need to add a request for a write lock before attempting to store.
    //
    // If the write lock request fails, you should return a status of RESOURCE_EXHAUSTED
    // and cancel the current operation.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::ALREADY_EXISTS - if the local cached file has not changed from the server version
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    File file;
    ClientContext context;
    StoreRes res;

    // set timeout
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));
    std::ifstream infile(WrapPath(filename));
//    std::cout << "File name: " << filename << std::endl;
    std::cout << "store File path: " << WrapPath(filename) << std::endl;
    if (!infile.is_open()) {
        std::cout << "Cannot open the file" << std::endl;
        return StatusCode::CANCELLED;
    }
    char buffer[BUFSIZ];

    // get the lock
    StatusCode status_code = RequestWriteAccess(filename);
    if (status_code == StatusCode::OK) {
        std::uint32_t crc = dfs_file_checksum(WrapPath(filename), &crc_table);
        file.set_crc(crc);
        file.set_clientid(client_id);
    } else {
        return status_code;
    }
    std::unique_ptr<ClientWriter<File>> writer{service_stub->StoreFile(&context, &res)};
    while (!infile.eof()) {
        int bytes_read = infile.read(buffer, BUFSIZ).gcount();

        std::cout << "Bytes Read: " << bytes_read << std::endl;

        file.set_name(filename);
        file.set_size(bytes_read);
        file.set_content(buffer, bytes_read);
        writer->Write(file);
    }
    infile.close();
    writer->WritesDone();
    Status status = writer->Finish();
//    std::cout << "File content: " << file.content() << std::endl;
    if (status.ok()) {
        return StatusCode::OK;
    } else if (status.error_code() == 4) {
        return StatusCode::DEADLINE_EXCEEDED;
    } else if (status.error_code() == 6) {
        return StatusCode::ALREADY_EXISTS;
    } else {
        std::cout << "store Error: " << status.error_message() << std::endl;
        return StatusCode::CANCELLED;
    }
}



grpc::StatusCode DFSClientNodeP2::Fetch(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation. However, you will
    // need to adjust this method to recognize when a file trying to be
    // fetched is the same on the client (i.e. the files do not differ
    // between the client and server and a fetch would be unnecessary.
    //
    // The StatusCode response should be:
    //
    // OK - if all went well
    // DEADLINE_EXCEEDED - if the deadline timeout occurs
    // NOT_FOUND - if the file cannot be found on the server
    // ALREADY_EXISTS - if the local cached file has not changed from the server version
    // CANCELLED otherwise
    //
    // Hint: You may want to match the mtime on local files to the server's mtime
    //
    File file;
    ClientContext context;
    FetchRequest request;
    StatusRequest status_request;
    StatusRes status_res;
    ClientContext status_context;

    // set timeout
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));

    // get the file status
    status_request.set_name(filename);
    std::cout << "fetch File name is: " << filename << std::endl;
    std::cout << "File path is: " << WrapPath(filename) << std::endl;
    std::cout << "crc table is: " << &crc_table << std::endl;
    Status file_status = service_stub->StatusFile(&status_context, status_request, &status_res);
    std::uint32_t crc = dfs_file_checksum(WrapPath(filename), &crc_table);
    std::cout << "crc is: " << crc << std::endl;
    std::cout << "status_crc is: " << status_res.crc() << std::endl;

    if (status_res.crc() == crc) {
        return StatusCode::ALREADY_EXISTS;
    }

    std::ofstream outfile(WrapPath(filename));
    std::cout << "File path: " << WrapPath(filename) << std::endl;
    if (!outfile.is_open()) {
        std::cout << "Cannot open the file" << std::endl;
        return StatusCode::CANCELLED;
    }

    // get the file
    request.set_name(filename);
    std::unique_ptr<ClientReader<File>> reader{service_stub->FetchFile(&context, request)};

    while (reader->Read(&file)) {
        outfile.write(file.content().c_str(), file.size());
        std::cout << "reader File name: " << filename << std::endl;
    }

    Status status = reader->Finish();
    std::cout << "status code: " << status.error_code() << std::endl;
    if (status.ok()) {
        return StatusCode::OK;
    } else if (status.error_code() == 4) {
        return StatusCode::DEADLINE_EXCEEDED;
    } else if (status.error_code() == 5) {
        return StatusCode::NOT_FOUND;
    } else {
        std::cout << "Error: " << status.error_code() << std::endl;
        return StatusCode::CANCELLED;
    }
}

grpc::StatusCode DFSClientNodeP2::Delete(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to delete a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You will also need to add a request for a write lock before attempting to delete.
    //
    // If the write lock request fails, you should return a status of RESOURCE_EXHAUSTED
    // and cancel the current operation.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //
    DeleteRequest request;
    DeleteRes response;
    ClientContext context;

    // set timeout
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));

    // try to get the lock
    StatusCode status_code = RequestWriteAccess(filename);
    if (status_code != StatusCode::OK) {
        return status_code;
    }

    // Continue for Delete
    request.set_name(filename);
    request.set_clientid(ClientId());
    std::cout << "Delete filename: " << filename << std::endl;
    Status status = service_stub->DeleteFile(&context, request, &response);
    if (status.ok()) {
        std::cout << "delete file ok" << std::endl;
        return StatusCode::OK;
    } else if (status.error_code() == 4) {
        return StatusCode::DEADLINE_EXCEEDED;
    } else if (status.error_code() == 5) {
        return StatusCode::NOT_FOUND;
    } else {
        std::cout << "Error: " << status.error_message() << std::endl;
        return StatusCode::CANCELLED;
    }
}

grpc::StatusCode DFSClientNodeP2::List(std::map<std::string,int>* file_map, bool display) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list files here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // listing details that would be useful to your solution to the list response.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::CANCELLED otherwise
    //
    //
    ListRequest request;
    ListRes response;
    ClientContext context;

    // set timeout
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));

    Status status = service_stub->ListFile(&context, request, &response);
    if (status.ok()) {
        for (const FileInfo &fileinfo : response.fileinfo()) {
            file_map->insert(std::make_pair(fileinfo.name(), fileinfo.modifiedtime()));
            std::cout << "file name: " << fileinfo.name() << std::endl;
            std::cout << "modified time: " << fileinfo.modifiedtime() << std::endl;
        }

        return StatusCode::OK;
    } else if (status.error_code() == 4) {
        return StatusCode::DEADLINE_EXCEEDED;
    } else {
        std::cout << "Error: " << status.error_message() << std::endl;
        return StatusCode::CANCELLED;
    }
}

grpc::StatusCode DFSClientNodeP2::Stat(const std::string &filename, void* file_status) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // status details that would be useful to your solution.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //
    StatusRequest request;
    StatusRes response;
    ClientContext context;

    // set timeout
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));

    request.set_name(filename);
    Status status = service_stub->StatusFile(&context, request, &response);

    if (status.ok()) {
        std::cout << "File name: " << response.name() << std::endl;
        std::cout << "File size: " << response.size() << std::endl;
        std::cout << "File mtime: " << response.modifiedtime() << std::endl;
        std::cout << "File ctime: " << response.createdtime() << std::endl;
    }
    if (status.ok()) {
      return StatusCode::OK;
    } else if (status.error_code() == 4) {
        return StatusCode::DEADLINE_EXCEEDED;
    } else {
        std::cout << "Error: " << status.error_message() << std::endl;
        return StatusCode::CANCELLED;
    }
}

void DFSClientNodeP2::InotifyWatcherCallback(std::function<void()> callback) {

    //
    // STUDENT INSTRUCTION:
    //
    // This method gets called each time inotify signals a change
    // to a file on the file system. That is every time a file is
    // modified or created.
    //
    // You may want to consider how this section will affect
    // concurrent actions between the inotify watcher and the
    // asynchronous callbacks associated with the server.
    //
    // The callback method shown must be called here, but you may surround it with
    // whatever structures you feel are necessary to ensure proper coordination
    // between the async and watcher threads.
    //
    // Hint: how can you prevent race conditions between this thread and
    // the async thread when a file event has been signaled?
    //

    std::lock_guard<std::mutex> lock(mutex);
    callback();

}

//
// STUDENT INSTRUCTION:
//
// This method handles the gRPC asynchronous callbacks from the server.
// We've provided the base structure for you, but you should review
// the hints provided in the STUDENT INSTRUCTION sections below
// in order to complete this method.
//
void DFSClientNodeP2::HandleCallbackList() {

    void* tag;
    bool ok = false;

    //
    // STUDENT INSTRUCTION:
    //
    // Add your file list synchronization code here.
    //
    // When the server responds to an asynchronous request for the CallbackList,
    // this method is called. You should then synchronize the
    // files between the server and the client based on the goals
    // described in the readme.
    //
    // In addition to synchronizing the files, you'll also need to ensure
    // that the async thread and the file watcher thread are cooperating. These
    // two threads could easily get into a race condition where both are trying
    // to write or fetch over top of each other. So, you'll need to determine
    // what type of locking/guarding is necessary to ensure the threads are
    // properly coordinated.
    //

    // Block until the next result is available in the completion queue.
    while (completion_queue.Next(&tag, &ok)) {
        {
            //
            // STUDENT INSTRUCTION:
            //
            // Consider adding a critical section or RAII style lock here
            //
            std::lock_guard<std::mutex> lock(mutex);
            // The tag is the memory location of the call_data object
            AsyncClientData<FileListResponseType> *call_data = static_cast<AsyncClientData<FileListResponseType> *>(tag);

            dfs_log(LL_DEBUG2) << "Received completion queue callback";

            // Verify that the request was completed successfully. Note that "ok"
            // corresponds solely to the request for updates introduced by Finish().
            // GPR_ASSERT(ok);
            if (!ok) {
                dfs_log(LL_ERROR) << "Completion queue callback not ok.";
            }

            if (ok && call_data->status.ok()) {

                dfs_log(LL_DEBUG3) << "Handling async callback ";

                //
                // STUDENT INSTRUCTION:
                //
                // Add your handling of the asynchronous event calls here.
                // For example, based on the file listing returned from the server,
                // how should the client respond to this updated information?
                // Should it retrieve an updated version of the file?
                // Send an update to the server?
                // Do nothing?
                //
                auto &response = call_data->reply;
                for (const StatusRes &statusres : response.statusres()) {
                    if (access(WrapPath(statusres.name()).c_str(), R_OK)) {
                        // file exist, compare the time
                        struct stat buf;
                        std::uint32_t crc = dfs_file_checksum(WrapPath(statusres.name()), &crc_table);
                        stat(WrapPath(statusres.name()).c_str(), &buf);
                        if (statusres.crc() != crc) {
                            if (statusres.modifiedtime() < buf.st_mtime) {
                                Store(statusres.name());
                            } else {
                                Fetch(statusres.name());
                            }
                        }
                        if (statusres.crc() == 0) {
                            Delete(WrapPath(statusres.name()));
                        }
                    } else {
                        // file not exist in client, just fetch it from the server
                        Fetch(statusres.name());
                    }
                }


            } else {
                dfs_log(LL_ERROR) << "Status was not ok. Will try again in " << DFS_RESET_TIMEOUT << " milliseconds.";
                dfs_log(LL_ERROR) << call_data->status.error_message();
                std::this_thread::sleep_for(std::chrono::milliseconds(DFS_RESET_TIMEOUT));
            }

            // Once we're complete, deallocate the call_data object.
            delete call_data;

            //
            // STUDENT INSTRUCTION:
            //
            // Add any additional syncing/locking mechanisms you may need here

        }


        // Start the process over and wait for the next callback response
        dfs_log(LL_DEBUG3) << "Calling InitCallbackList";
        InitCallbackList();

    }
}

/**
 * This method will start the callback request to the server, requesting
 * an update whenever the server sees that files have been modified.
 *
 * We're making use of a template function here, so that we can keep some
 * of the more intricate workings of the async process out of the way, and
 * give you a chance to focus more on the project's requirements.
 */
void DFSClientNodeP2::InitCallbackList() {
    CallbackList<FileRequestType, FileListResponseType>();
}

//
// STUDENT INSTRUCTION:
//
// Add any additional code you need to here
//


