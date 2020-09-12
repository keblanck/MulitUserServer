# MultiUserChatServer
A multi-connection command line chat server.

This server-client model is programmed in C and uses sockets, ports, pipes and process-forking
within the TCP/IP model to connect as many clients as desired via a central server. Users can 
specify the host and the port number used to customize their connection. All chat happens via 
the command line rather than a GUI.
