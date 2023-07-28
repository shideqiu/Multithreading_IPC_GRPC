GRPC and Distributed Systems

## Foreward

In this project, you will design and implement a simple distributed file system (DFS). First, you will develop several file transfer protocols using gRPC and Protocol Buffers. Next, you will incorporate a weakly consistent synchronization system to manage cache consistency between multiple clients and a single server. The system should be able to handle both binary and text-based files.

Your source code will use a combination of C++14, gRPC, and Protocol Buffers to complete the implementation.

## Setup

You can clone the code in the Project 4 repository with the command:

```
git clone https://github.com/shideqiu/Multithreading_IPC_GRPC.git
```

## Directions

- Directions for Part 1 can be found in [docs/part1.md](docs/part1.md)
- Directions for Part 2 can be found in [docs/part2.md](docs/part2.md)

## Log Utility

We've provided a simple logging utility, `dfs_log`, in this assignment that can be used within your project files.

There are five log levels (LL_SYSINFO, LL_ERROR, LL_DEBUG, LL_DEBUG2, and LL_DEBUG3).

During the tests, only log levels LL_SYSINFO, LL_ERROR, and LL_DEBUG will be output. The others will be ignored. You may use the other log levels for your own debugging and testing.

The log utility uses a simple streaming syntax. To use it, make the function call with the log level desired, then stream your messages to it. For example:

```
dfs_log(LL_DEBUG) << "Type your message here: " << add_a_variable << ", add more info, etc."
```

## Rubric

Your project will be graded at least on the following items:

- Interface specification (.proto)
- Service implementation
- gRPC initiation and binding
- Proper handling of deadline timeouts
- Proper clean up of memory and gRPC resources
- Proper communication with the server
- Proper request and management of write locks
- Proper synchronization of files between multiple clients and a single server
- Insightful observations in the Readme file and suggestions for improving the class for future semesters

#### gRPC Implementation

Full credit requires: code compiles successfully, does not crash, files fully transmitted, basic safety checks, and proper use of gRPC - including the ability to get, store, and list files, along with the ability to recognize a timeout. Note that the automated tests will test some of these automatically, but graders may execute additional tests of these requirements.

#### DFS Implementation

Full credit requires: code compiles successfully, does not crash, files fully transmitted, basic safety checks, proper use of gRPC, write locks properly handled, cache properly handled, synchronization of sync and inotify threads properly handled, and synchronization of multiple clients to a single server. Note that the automated tests will test some of these automatically, but graders may execute additional tests of these requirements.

## [README](GRPC_Readme.pdf)

- Clearly demonstrates my understanding of what I did and why.
- A description of the flow of control for my code.
- A brief explanation of how you tested your code. (2 points)
- References any external materials that I consulted during my
  development process (1 point).

## References

### Relevant lecture material

- [P4L1 Remote Procedure Calls](https://gatech.instructure.com/courses/270294/pages/p4l1-remote-procedure-calls?module_item_id=2666700)

### gRPC and Protocol Buffer resources

- [gRPC C++ Reference](https://grpc.github.io/grpc/cpp/index.html)
- [Protocol Buffers 3 Language Guide](https://developers.google.com/protocol-buffers/docs/proto3)
- [gRPC C++ Examples](https://github.com/grpc/grpc/tree/master/examples/cpp)
- [gRPC C++ Tutorial](https://grpc.io/docs/tutorials/basic/cpp/)
- [Protobuffers Scalar types](https://developers.google.com/protocol-buffers/docs/proto3#scalar)
- [gRPC Status Codes](https://github.com/grpc/grpc/blob/master/doc/statuscodes.md)
- [gRPC Deadline](https://grpc.io/blog/deadlines/)
