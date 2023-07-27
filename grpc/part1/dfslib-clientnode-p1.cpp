#include <regex>
#include <vector>
#include <string>
#include <cstdio>
#include <chrono>
#include <errno.h>
#include <iostream>
#include <thread>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <csignal>
#include <sys/inotify.h>
#include <grpcpp/grpcpp.h>

#include "dfslib-shared-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"
#include "dfslib-clientnode-p1.h"

using grpc::Channel;
using grpc::Status;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientContext;
using grpc::ClientReader;

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
//
// STUDENT INSTRUCTION:
//
// You may want to add aliases to your namespaced service methods here.
// All of the methods will be under the `dfs_service` namespace.   
//
// For example, if you have a method named MyMethod, add
// the following:   
//
//      using dfs_service::MyMethod
//


DFSClientNodeP1::DFSClientNodeP1() : DFSClientNode() {}

DFSClientNodeP1::~DFSClientNodeP1() noexcept {}

StatusCode DFSClientNodeP1::Store(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to store a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept and store a file.
    //
    // When working with files in gRPC you'll need to stream
    // the file contents, so consider the use of gRPC's ClientWriter.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the client
    // StatusCode::CANCELLED otherwise
    //
    //
    File file;
    ClientContext context;
    StoreRes res;

    // set timeout
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));
    std::ifstream infile(WrapPath(filename));
//    std::cout << "File name: " << filename << std::endl;
    std::cout << "File path: " << WrapPath(filename) << std::endl;
    if (!infile.is_open()) {
        std::cout << "Cannot open the file" << std::endl;
        return StatusCode::CANCELLED;
    }
    char buffer[BUFSIZ];
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
    } else {
      std::cout << "Error: " << status.error_message() << std::endl;
      return StatusCode::CANCELLED;
    }
}


StatusCode DFSClientNodeP1::Fetch(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept a file request and return the contents
    // of a file from the service.
    //
    // As with the store function, you'll need to stream the
    // contents, so consider the use of gRPC's ClientReader.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //
    File file;
    ClientContext context;
    FetchRequest request;

    // set timeout
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));
    std::ofstream outfile(WrapPath(filename));
    std::cout << "File path: " << WrapPath(filename) << std::endl;
    if (!outfile.is_open()) {
        std::cout << "Cannot open the file" << std::endl;
    }
    request.set_name(filename);
    std::unique_ptr<ClientReader<File>> reader{service_stub->FetchFile(&context, request)};
    while (reader->Read(&file)) {
        outfile.write(file.content().c_str(), file.size());
//        std::cout << "File name: " << filename << std::endl;
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
        std::cout << "Error: " << status.error_message() << std::endl;
        return StatusCode::CANCELLED;
    }

}

StatusCode DFSClientNodeP1::Delete(const std::string& filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to delete a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //
    DeleteRequest request;
    DeleteRes response;
    ClientContext context;

    // set timeout
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));

    request.set_name(filename);
    Status status = service_stub->DeleteFile(&context, request, &response);
    if (status.ok()) {
        return StatusCode::OK;
    } else if (status.error_code() == 5) {
        return StatusCode::NOT_FOUND;
    } else if (status.error_code() == 4) {
        return StatusCode::DEADLINE_EXCEEDED;
    } else {
        std::cout << "Error: " << status.error_message() << std::endl;
        return StatusCode::CANCELLED;
    }
}


StatusCode DFSClientNodeP1::Stat(const std::string &filename, void* file_status) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. This method should
    // retrieve the status of a file on the server. Note that you won't be
    // tested on this method, but you will most likely find that you need
    // a way to get the status of a file in order to synchronize later.
    //
    // The status might include data such as name,  size, mtime, crc, etc.
    //
    // The file_status is left as a void* so that you can use it to pass
    // a message type that you defined. For instance, may want to use that message
    // type after calling Stat from another method.
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
        std::cout << "File name is: " << response.name() << std::endl;
        std::cout << "File size is: " << response.size() << std::endl;
        std::cout << "File mtime is: " << response.modifiedtime() << std::endl;
        std::cout << "File ctime is: " << response.createdtime() << std::endl;
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

StatusCode DFSClientNodeP1::List(std::map<std::string,int>* file_map, bool display) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list all files here. This method
    // should connect to your service's list method and return
    // a list of files using the message type you created.
    //
    // The file_map parameter is a simple map of files. You should fill
    // the file_map with the list of files you receive with keys as the
    // file name and values as the modified time (mtime) of the file
    // received from the server.
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
//
// STUDENT INSTRUCTION:
//
// Add your additional code here, including
// implementations of your client methods
//

