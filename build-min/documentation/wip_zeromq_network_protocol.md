

# Overview 

NOTE: This guide is a work in progress.

DICOMautomaton uses ZeroMQ for distributed computing. The protocol is described here.

## Rationale

This network protocol is designed to be extensible and support a variety of distributed computing use-cases. It is meant
to help simplify coordination and development of worker nodes.

## Broad Design Constraints

- Workers are expected to have varying capabilities. In particular, workers may support an extremely limited number of
  operations compared to a complete DICOMautomaton installation.

- Workers are expected to appear and disappear regularly, allowing for flexibility in network configurations.

- To simplify data interchange, objects on-the-wire will be serialized to well-known formats. This is slower than making
  a custom serialization format, but dramatically simplifies potential 'gotchas', like endianness, protocol updates, and
  the inclusion and correction association of metadata.

- The protocol will be designed to maximize the potential for re-use of components within DICOMautomaton.

- The protocol will be designed to simplify implementation of workers as much as possible.

## Protocol, version 0

In the following, a single 'primary' instance of DICOMautomaton is referred to as (1) whereas a collection of one or
more worker nodes are referred to as (2). The worker nodes could be other DICOMautomaton instances.

### Initial Handshake

- Primary instance (1) binds to a TCP port with an external-facing multiplexing proxy with a DEALER ZeroMQ socket.
  It uses a TCP protocol, address(es) to bind to, and a user-provided port number.

- A worker (2) connects to the proxy with a REP ZeroMQ socketand waits

- Primary instance (1) asks each connected worker (2) to 'advertise' it's capabilities.
  - The worker (2) responds with the following:
    - the DICOMautomaton network protocol version it wants to use
    - what data it is willing to accept, and in what form (e.g., multiple image arrays, whole single image array, an
      individual image, etc).
    - what operations it supports
    - whether the node can (or is willing to) send results back

- Primary instance (1) evaluates the requested network protocol version and either:
  - replies with a 'GOODBYE' if the version or capabilities are not accepted, or
  - replies with an identity 'ID123...' to precede all future network frames during all future communication

- The worker (2) responds with a 'WAITING'


### Work Parcelling

- Primary instance (1) prepares to distribute work by:
  - ensuring the requisite number of workers have connected
  - gathering information about available workers (2) during initial handshakes

  - gathering work instructions (e.g., a DICOMautomaton script)
  - gathering information about whether data is needed back after the remote procedure call

  - partitioning the data, one partition per worker, so that the worker has exclusive access to the data
    - the partition order is retained on the primary instance (1)


### Work Dispatch

- Eventually, the primary instance (1) proposes a work package to the worker (2).
  - Primary instance (1) confirms the worker (2) is still present with a 'KNOCK'
    - The worker (2) responds with a 'KNOCK'

  - Primary instance (1) sends the operation instruction
    - The worker (2) responds with either an 'ACCEPT' or 'DECLINE'
      - If 'DECLINE' the worker will be ignored (somehow???)

  - Primary instance (1) sends the data in multiple chunks
    - Each chunk starts with 'CHUNK' and an increasing chunk number
    - The worker (2) responds to each chunk with an 'ACK'

  - Primary instance (1) sends a 'WAITING'

  - The worker (2) performs the operation

  - The worker (2) confirms primary instance (1) is still present with a 'KNOCK2'
    - Primary instance (1) responds with a 'KNOCK2'

  - The worker (2) optionally sends the data in multiple chunks
    - Each chunk starts with 'CHUNK' and an increasing chunk number, which can be different than the previous chunk number
    - Primary instance (1) responds to each chunk with an 'ACK'

  - The worker (2) either:
    - sends a 'GOODBYE' if it intends to terminate, or
    - sends a 'WAITING' if it intends to awaiting another work dispatch

  - The primary instance (1) either:
    - sends an 'ACK' and keeps the connection open, or
    - sends a 'GOODBYE' and closes the connection if no further work is expected

  - The worker (2) does not respond, and either:
    - disconnects, or 
    - stays connected awaiting another work dispatch

  - If (and only if) data chunks were both expected and received, the primary instance (1) replaces the corresponding
    partition's data with any data received by the worker (2)

### Issues

- Protocol version 0 involves a *lot* of back-and-forth communication. Using a multiplexing proxy server to support this
  communication on a single channel will require a bidirectional naming system. It would also be difficult on a single
  channel to handle multiple workers, since with ZeroMQ out-of-the-box only a single stable REQ-REP is guaranteed;
  additional REQ-REP may be sent to other workers.

  Here are some other options:

  - Option A:

    Using two or more channels: one for coordination and registration, and one or more for direct
    connections between the Primary instance (1) and workers (2). The major downside with multiple channels is that
    additional ports and/or protocols are required. Clients may also need to be asynchronous, which is a downside for
    minimal or simplistic clients.

    Focusing on two channels only:

    - The 'coordination' channel can be 'chatty' and can be used to register workers, query capabilities, perform
      heartbeats, and offer jobs with instructions for accessing data.

      - Heartbeats during job processing still requires asynchronicity.

      - Can include a 'currently working on XYZ' field to track job status.
    
    - The 'data' channel can be more like a simple web server, serving requests for data and accepting replacements.
      This server has a well-defined role and probably won't need to be extended much in the future.

    Upsides:

    1. Work can be fanned out to multiple workers concurrently.

    2. There is clear separation between the coordination mechanism and the data processing mechanism.
       Both channels are free to have different semantics, e.g. protcols, socket types, handshakes, heartbeats, etc.

    3. The data server can be made somewhat generic and separated into it's own component, similar to a PACS.
    
    4. Coordination communications and heartbeats can be fit into a single REQ-REP, reducing the amount of 'state' each
       discussion requires (but we still need to track which jobs have successfully completed). Similarly, data requests
       can be split into atomic operations, e.g., 'get data X', and 'update data Y'.

    Downsides:

    1. Requires multiple sockets, likely multiple port numbers.

    2. Requires clients to speak two different protocols.

    3. Likely requires concurrency / asynchronicity in the client to maintain heartbeats on the coordination channel
       while communicating jobs on the data channel.

  - Option B:

    Implement a layer of name- and capability-based multiplexing on top of the ZeroMQ
    REQ-ROUTER-DEALER-REP paradigm.

    Major downsides are that a custom protocol would be needed, which might be harder for simple clients that
    asynchronicity.

    Upsides:

    1. All communication can happen on a single channel, so only a single port number and socket are needed.

    2. Work can be fanned out to multiple workers concurrently.

    Downsides:

    1. The same channel will be used to multiplex communication for all workers, so that 'state' of each 

    2. Concurrency / asynchronicity are required for coordination, job distribution, heartbeats, etc.

    3. The coordination mechanism and data mechanism will be tightly coupled, making the system less re-usable (i.e.,
       cannot be used as part of a PACS).

  - Option C:

    Though it might be slow and potentially cause resource exhaustion, is to force the entire version 0 protocol
    into a single REQ-REP handshake. This would simplify the routing problem, since the server would reach out to
    waiting clients with a list of requirements *and* the packaged work, and the worker would only need to respond once
    with either a DECLINE or the final result.
    
    Upsides:

    1. There is no need to worry about synchronizing multiple workers; the prinary instance (1) can focus on one at a
    time.

    Downsides:

    1. It will be difficult to differentiate long-running tasks from disconnects, depending on the socket type.

    2. Clients unable to process jobs will be offered the full data, which could be large and time consuming to send.

    3. Clients may repeatedly be offered the same job and data due to ZeroMQs fair routing approach.

    4. Throughput will be poor since the primary instance (1) waits for each worker, one at a time.

  Option A seems like the most sensible option overall.

