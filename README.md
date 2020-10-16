# among-us-ste-edition
Among Us Save-The-Environment Edition

prior to running, on linux, install:

sudo apt update

sudo apt upgrade

sudo apt-get install build-essential libglu1-mesa-dev libpng-dev libasound2-dev

also include the asio library into this project.


1. Database: live on the server, synced across all clients
- Any change to the database will be reflected on all clients
- Based on `MessageType`, `MessageAll` will send message to all client (to update a key/value pair in the database)
- Acknowledgement (`MessageACK`) from each client. If no ACK => retry ~N times. After N times, disconnect client

2. Lobby: live only on the server
- Client can only access lobby ids
- Allow client to connect to sever and choose a lobby ids
- Spin up 1 game instance (1 database) for each lobby
- Each lobby will run on its own thread

3. Loop check client-server connectivity:
- Ping client every n ms
- If no ACK, then disconnect client

4. Client is the only one doing the rendering + game physics

5. Server is doing all the game logic (i.e. winning & losing conditions,..)

