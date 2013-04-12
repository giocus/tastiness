#include "tastiness.h"
#include "mongoose.git/mongoose.h"

struct mg_connection* gConn;

static void websocket_ready_handler(struct mg_connection *conn) {
  gConn = conn;
  printf("websocket client connected\n");
}

void cmd_openRom(char* data, int data_len);

static int websocket_data_handler(struct mg_connection *conn, int flags,
                                  char *data, size_t data_len) {
  char* colon = strnstr(data, ":", 16);
  if(colon == 0) {
      printf("ERROR: received invalid websocket frame (no command) - terminating connection\n");
      return 0;
  }
  char commandName[16];
  memcpy(commandName, data, colon-data);
  commandName[colon-data] = 0;

  printf("commandName: %s\n", commandName);
  char* commandData = colon+1;
  int commandDataLen = data_len - (commandData - data);

  if(0);
  else if(!strcmp(commandName, "openRom")) cmd_openRom(commandData, commandDataLen);
  else {
      printf("ERROR: unhandled websocket command '%s' - terminating connection\n", commandName);
      return 0;
  }

  // // Prepare frame
  // reply[0] = 0x82;  // binary
  // reply[1] = data_len;

  // // Copy message from request to reply, applying the mask if required.
  // for (i = 0; i < data_len; i++) {
  //   reply[i + 2] = data[i];
  // }

  // // Echo the message back to the client
  // mg_write(conn, reply, 2 + data_len);

  return 1;
}

int main(int argc, char** argv) {
  struct mg_context *ctx;
  struct mg_callbacks callbacks;
  const char *options[] = {
    "listening_ports", "9999",
    NULL
  };

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.websocket_ready = websocket_ready_handler;
  callbacks.websocket_data = websocket_data_handler;
  printf("Waiting for websocket connection on port 9999\n");
  ctx = mg_start(&callbacks, NULL, options);
  getchar();
  sleep(86400*365); //1 year

  return 0;
}

void cmd_openRom(char* data, int data_len) {
  static bool initialized = false;
  if(initialized) Emulator::Shutdown();
  initialized = true;

  char* romFilename = tempnam(0, "tastiness-rom-");
  printf("romFilename: %s length: %d\n", romFilename, data_len);
  FILE* fp = fopen(romFilename, "wb");
  fwrite(data, 1, data_len, fp);
  fclose(fp);
  Emulator::Initialize(romFilename);
  vector<uint8> save;
  Emulator::SaveUncompressed(&save);
  printf("Save states are %ld bytes.\n", save.size());
}
