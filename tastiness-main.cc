#include "tastiness.h"
#include "mongoose.git/mongoose.h"
#include <queue>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

typedef uint8 InputState;

static pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;

uint8* EMU_GFX;
int32* EMU_SOUND;
int32 EMU_SSIZE;


//timeline...
//gEmuInitialState + gInputTimeline[0] -> gEmuStateTimeline[0]
//gEmuStateTimeline[0] + gInputTimeline[1] -> gEmuStateTimeline[1]
vector<uint8> gEmuInitialState;
vector<vector<uint8> > gEmuStateTimeline;
vector<InputState> gInputTimeline;

vector<uint16> gRamBlackoutBytes;

lua_State* gLuaState;
vector<uint8> gLuaSource;
string gLuaLoadError;
string gLuaOutput;
bool gLuaCodeIsValid = false;

double gLuaAStarH;

struct mg_connection* gConn;

void websocket_ready_handler(struct mg_connection *conn);
void doStep(int frame);
void send_bin(mg_connection* conn, char* msgt, unsigned char* data, int data_len);

void cmd_openRom(mg_connection* conn, char* data, int data_len);
void cmd_getFrame(mg_connection* conn, char* data, int data_len);
void cmd_setFrameInputs(mg_connection* conn, char* data, int data_len);
void cmd_setRamBlackoutBytes(mg_connection* conn, char* data, int data_len);
void cmd_setLuaSource(mg_connection* conn, char* data, int data_len);
void cmd_initAStar(mg_connection* conn, char* data, int data_len);
void cmd_workAStar(mg_connection* conn, char* data, int data_len);

int websocket_data_handler(mg_connection *conn, int flags, char *data, size_t data_len) {
  pthread_mutex_lock(&gMutex);

  char* colon = strnstr(data, ":", 64);
  if(colon == 0) {
      printf("ERROR: received invalid websocket frame (no command) - terminating connection\n");
      pthread_mutex_unlock(&gMutex);
      return 0;
  }
  char commandName[64];
  memcpy(commandName, data, colon-data);
  commandName[colon-data] = 0;

  char* commandData = colon+1;
  int commandDataLen = data_len - (commandData - data);

  printf("thread_id: %08lx commandName: %s commandDataLen: %d\n", (long)pthread_self(), commandName, commandDataLen);

  uint64_t st = microTime();
  if(0);
#define CMD(n) else if(!strcmp(commandName, #n)) cmd_ ## n (conn, commandData, commandDataLen);
  CMD(openRom)
  CMD(getFrame)
  CMD(setFrameInputs)
  CMD(setRamBlackoutBytes)
  CMD(setLuaSource)
  CMD(initAStar)
  CMD(workAStar)
#undef CMD
  else {
      printf("ERROR: unhandled websocket command '%s' - terminating connection\n", commandName);
      pthread_mutex_unlock(&gMutex);
      return 0;
  }
  uint64_t et = microTime();
  printf("command took %.3fms\n", double(et-st)/1000);

  pthread_mutex_unlock(&gMutex);
  return 1;
}

void websocket_ready_handler(struct mg_connection *conn) {
  pthread_mutex_lock(&gMutex);

  gConn = conn;
  printf("websocket client connected\n");
  send_bin(conn, "binFrameInputs", &gInputTimeline[0], gInputTimeline.size());
  send_bin(conn, "binLuaSource", &gLuaSource[0], gLuaSource.size());
  send_bin(conn, "binRamBlackoutBytes", (uint8*)&gRamBlackoutBytes[0], sizeof(uint16) * gRamBlackoutBytes.size());

  pthread_mutex_unlock(&gMutex);
}

static int l_my_print(lua_State* L) {
    int nargs = lua_gettop(L);

    string printstring;

    for (int i=1; i <= nargs; i++) {
      lua_getglobal(L, "tostring");
      lua_insert(L, -2);
      int error = lua_pcall(L, 1, 1, 0);
      string s;
      if(error) {
        s = string("<error invoking tostring: ") + lua_tostring(gLuaState, -1) + ">";
      } else {
        s = lua_tostring(L, -1);
      }
      lua_pop(L, 1);
      if(i>1) {
        printstring = string("\t") + printstring;
      }
      printstring = s + printstring;
    }
    gLuaOutput += printstring;
    gLuaOutput += '\n';

    return 0;
}

static int l_get_ram(lua_State* L) {
  double address = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  lua_pushnumber(L, RAM[((int)address) & 0xfff]);
  return 1;
}

static int l_set_astar(lua_State* L) {
  gLuaAStarH = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  return 1;
}

static const struct luaL_reg mylib [] = {
  {"print", l_my_print},
  {"ram", l_get_ram},
  {"setAStar", l_set_astar},
  {NULL, NULL}
};

int main(int argc, char** argv) {
  const char* port = "9999";
  if(argc > 1 && atoi(argv[1])) {
    port = argv[1];
  }
  struct mg_context *ctx;
  struct mg_callbacks callbacks;
  const char *options[] = {
    "listening_ports", port,
    "document_root", "html_ui",
    NULL
  };

  gLuaState = lua_open();
  luaL_openlibs(gLuaState);

  lua_getglobal(gLuaState, "_G");
  luaL_register(gLuaState, NULL, mylib);
  lua_pop(gLuaState, 1);

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.websocket_ready = websocket_ready_handler;
  callbacks.websocket_data = websocket_data_handler;
  printf("\n\nTAStiNESs waiting for connection: http://localhost:%s/\n\n", port);
  ctx = mg_start(&callbacks, NULL, options);

  while(true) { sleep(60); }

  return 0;
}

void cmd_openRom(mg_connection* conn, char* data, int data_len) {
  static bool initialized = false;
  if(initialized) Emulator::Shutdown();
  initialized = true;

  char* romFilename = tempnam(0, "tastiness-rom-");
  printf("romFilename: %s length: %d\n", romFilename, data_len);
  FILE* fp = fopen(romFilename, "wb");
  fwrite(data, 1, data_len, fp);
  fclose(fp);
  Emulator::Initialize(romFilename);

  Emulator::SaveUncompressed(&gEmuInitialState);
  printf("Save states are %ld bytes.\n", gEmuInitialState.size());

  gEmuStateTimeline.clear();
}

struct BITMAPINFOHEADER {
  uint32 biSize;
  uint32 biWidth;
  uint32 biHeight;
  uint16 biPlanes;
  uint16 biBitCount;
  uint32 biCompression;
  uint32 biSizeImage;
  uint32 biXPelsPerMeter;
  uint32 biYPelsPerMeter;
  uint32 biClrUsed;
  uint32 biClrImportant;
};

void send_header(mg_connection* conn, bool binary, int data_len) {
  char header[64];
  char* c = header;
  if(binary) {
    *c++ = 0x82;
  } else {
    *c++ = 0x81;
  }
  if(data_len >= 65536) {
    *c++ = 0x7f; //size coming, 8 bytes
    ((uint32*)c)[0] = 0;
    c+=4;
    ((uint32*)c)[0] = htonl(data_len);
    c+=4;
  } else if(data_len >= 126) {
    *c++ = 0x7e; //size coming, 2 bytes
    ((uint16*)c)[0] = htons(data_len);
    c+=2;
  } else {
    *c++ = data_len;
  }
  mg_write(conn, header, c-header);
}

void send_txt(mg_connection* conn, char* msg) {
  int len = (int)strlen(msg);
  send_header(conn, false, len);
  mg_write(conn, msg, len);
}

void send_bin(mg_connection* conn, char* msgt, unsigned char* data, int data_len) {
  send_txt(conn, msgt);
  send_header(conn, true, data_len);
  mg_write(conn, data, data_len);
}


const int WIDTH = 256;
const int HEIGHT = 224;

uint8 getInputStateForFrame(int frame) {
  if(frame>=0 && frame < gInputTimeline.size()) {
    return gInputTimeline[frame];
  } else  {
    return 0;
  }
}

void cmd_setFrameInputs(mg_connection* conn, char* data, int data_len) {
  int invalidateFromIndex = gEmuStateTimeline.size();
  for(int i=0; i<data_len; i++) {
    uint8 input = ((uint8*)data)[i];
    if(i>=gInputTimeline.size() || gInputTimeline[i] != input) {
      invalidateFromIndex = i;
      break;
    }
  }
  gInputTimeline.resize(data_len);
  memcpy(&gInputTimeline[0], data, data_len);
  if(gEmuStateTimeline.size() > invalidateFromIndex) {
    printf("invalidating from index %d\n", invalidateFromIndex);
    gEmuStateTimeline.resize(invalidateFromIndex);
  }
  printf("gEmuStateTimeline.size(): %d\n", (int)gEmuStateTimeline.size());
}

void cmd_setRamBlackoutBytes(mg_connection* conn, char* data, int data_len) {
  gRamBlackoutBytes.resize(data_len/2);
  memcpy(&gRamBlackoutBytes[0], data, data_len);
  printf("gRamBlackoutBytes.size(): %lu\n", gRamBlackoutBytes.size());
  for(size_t i=0; i<gRamBlackoutBytes.size(); i++) {
    printf("gRamBlackoutBytes[%lu]: %d\n", i, gRamBlackoutBytes[i]);
  }
  gEmuStateTimeline.clear();
}

void cmd_setLuaSource(mg_connection* conn, char* data, int data_len) {
  gLuaSource.resize(data_len);
  memcpy(&gLuaSource[0], data, data_len);

  int error = luaL_loadbuffer(gLuaState, (const char*)&gLuaSource[0], gLuaSource.size(), "luaSource");
  if(error) {
    gLuaLoadError = string("Error loading lua code:\n")+ lua_tostring(gLuaState, -1);
    puts(gLuaLoadError.c_str());
    lua_pop(gLuaState, 1);
    gLuaCodeIsValid = false;
  } else {
    printf("loaded lua code\n");
    lua_setglobal(gLuaState, "LuaFrameFunc");
    gLuaLoadError = "";
    gLuaCodeIsValid = true;
  }
}

struct LuaScriptResult {
  string output;
  double aStarH;
};
void runLuaAgainstCurrentEmulatorState(LuaScriptResult& result) {
  result.aStarH = gLuaAStarH = 1e6;
  if(!gLuaLoadError.empty()) {
    result.output = gLuaLoadError;
  } else if(gLuaCodeIsValid) {
    gLuaOutput.clear();
    lua_getglobal(gLuaState, "LuaFrameFunc");
    int error = lua_pcall(gLuaState, 0, 0, 0);
    if(error) {
      result.output = string("Error running lua code:\n") + lua_tostring(gLuaState, -1);
      lua_pop(gLuaState, 1);
    } else {
      result.aStarH = gLuaAStarH;
      result.output = gLuaOutput;
    }
  }
}

uint64 getRamHashForCurrentEmulatorState() {
  uint8 ramForHash[0x800];
  memcpy(ramForHash, RAM, 0x800);
  for(size_t i=0; i<gRamBlackoutBytes.size(); i++) {
    ramForHash[gRamBlackoutBytes[i]] = 0;
  }
  return CityHash64((const char*)ramForHash, 0x800);
}

struct NextNodeInfo {
  uint64 hash;
  double aStarH;
};

void getDistinctNextNodesFromCurrentEmulatorState(map<InputState, NextNodeInfo>& nextNodeInfos) {
  LuaScriptResult scriptResult;
  set<uint64> foundHashes;

  vector<uint8> thisState;
  Emulator::SaveUncompressed(&thisState);
  for(int inputState=0; inputState<256; inputState++) {
    Emulator::LoadUncompressed(&thisState);
    Emulator::Step(inputState, &EMU_GFX, &EMU_SOUND, &EMU_SSIZE, 2);
    uint64 hash = getRamHashForCurrentEmulatorState();
    if(foundHashes.count(hash) == 0) {
      foundHashes.insert(hash);
      runLuaAgainstCurrentEmulatorState(scriptResult);
      nextNodeInfos[inputState].hash = hash;
      nextNodeInfos[inputState].aStarH = scriptResult.aStarH;
    }
  }
}

struct AStar {
  struct Node {
    boost::shared_ptr<vector<uint8> > emuState; //this is cleared when the node enters the closed set
    double f; //estimated cost to goal
    uint32 g; //cost from the start node
    double v() const { return f+g; }
  };

  class CompareHashesByNodeGplusH {
    AStar& astar;
  public:
    CompareHashesByNodeGplusH(AStar& astar) : astar(astar) {}
    bool operator()(uint64 a, uint64 b) const {
      return astar.nodesByHash[a].v() > astar.nodesByHash[b].v();
    }
  };

  boost::unordered_map<uint64, Node> nodesByHash;
  priority_queue<uint64, vector<uint64>, CompareHashesByNodeGplusH> openHashPriorities;
  set<uint64> openHashes;
  set<uint64> closedHashes;
  map<uint64, uint64> hashParents;
  vector<uint8> startEmuState;

  AStar() : openHashPriorities(CompareHashesByNodeGplusH(*this)) {}

  struct Status {
    uint64 openNodes;
    uint64 closedNodes;
    vector<InputState> pathToBestNode;
  };
  Status getStatus() {
    Status s;
    s.openNodes = openHashes.size();
    s.closedNodes = closedHashes.size();
    getInputStatePath(s.pathToBestNode, openHashPriorities.top());
    return s;
  }

  void getInputStatePath(vector<InputState>& result, uint64 childNodeHash) {
    Emulator::LoadUncompressed(&startEmuState);
    uint64 startHash = getRamHashForCurrentEmulatorState();

    vector<uint64> reverseHashPath;
    uint64 hash = childNodeHash;
    while(hash != startHash) {
      reverseHashPath.push_back(hash);
      hash = hashParents[hash];
      CHECK(hash != 0 && "parent hash not found");
    }

    vector<uint8> curState = startEmuState;

    result.clear();
    for(int i=reverseHashPath.size()-1; i>=0; i--) {
      uint64 wantHash = reverseHashPath[i];
      int inputState;
      for(inputState=0; inputState<256; inputState++) {
        Emulator::LoadUncompressed(&curState);
        Emulator::Step(inputState, &EMU_GFX, &EMU_SOUND, &EMU_SSIZE, 2);
        uint64 newHash = getRamHashForCurrentEmulatorState();
        if(newHash == wantHash) {
          result.push_back(inputState);
          Emulator::SaveUncompressed(&curState);
          break;
        }
      }
      CHECK(inputState != 256 && "could not find inputState");
    }
  }

  void addCurrentEmulatorStateAsNewNode(uint64 hash=0, uint32 g=0) {
    vector<uint8>* emuState = new vector<uint8>();
    Emulator::SaveUncompressed(emuState);

    LuaScriptResult scriptResult;
    runLuaAgainstCurrentEmulatorState(scriptResult);
    if(scriptResult.aStarH < 0) {
      CHECK(false && "handle goal complete");
    }

    Node n;
    n.emuState = boost::shared_ptr<vector<uint8> >(emuState);
    n.g = g;
    n.f = scriptResult.aStarH;

    if(hash == 0) {
      hash = getRamHashForCurrentEmulatorState();
    }

    nodesByHash.insert(boost::unordered_map<uint64, Node>::value_type(hash, n));
    openHashes.insert(hash);
    openHashPriorities.push(hash);
  }

  void initFromCurrentEmulatorState() {
    Emulator::SaveUncompressed(&startEmuState);
    addCurrentEmulatorStateAsNewNode();
  }

  void work1() {
    uint64 oldHash = openHashPriorities.top();
    openHashPriorities.pop();
    openHashes.erase(oldHash);
    closedHashes.insert(oldHash);

    Node& oldNode = nodesByHash[oldHash];

    CHECK(oldNode.emuState && "nodes in the open set should have emuState");

    for(int inputState=0; inputState<256; inputState++) {
      Emulator::LoadUncompressed(oldNode.emuState.get());
      Emulator::Step(inputState, &EMU_GFX, &EMU_SOUND, &EMU_SSIZE, 2);
      uint64 newHash = getRamHashForCurrentEmulatorState();
      if(openHashes.count(newHash) != 0) continue; //already in open set
      if(closedHashes.count(newHash) != 0) {
        //in the closed set means the node has been visited, but now we have
        //to check to see if we have improved G
        CHECK(false && "handle re-finding node in the closed set");
      } else {
        addCurrentEmulatorStateAsNewNode(newHash, oldNode.g+1);
        hashParents[newHash] = oldHash;
      }
    }
  }
};

boost::shared_ptr<AStar> gAStar;

void emulateToFrame(int frame) {
  //printf("getFrame() frame: %d gEmuStateTimeline.size(): %d\n", frame, gEmuStateTimeline.size());
  //step with frameskip as needed
  if(gEmuStateTimeline.size() <= frame) {
    if(gEmuStateTimeline.empty()) {
      Emulator::LoadUncompressed(&gEmuInitialState);
    } else {
      Emulator::LoadUncompressed(&gEmuStateTimeline[gEmuStateTimeline.size()-1]);
    }
    for(int f=gEmuStateTimeline.size(); f<frame; f++) {
      Emulator::Step(getInputStateForFrame(f), &EMU_GFX, &EMU_SOUND, &EMU_SSIZE, 2);
      gEmuStateTimeline.resize(f+1);
      Emulator::SaveUncompressed(&gEmuStateTimeline[f]);
      //printf("frameskip stepped into frame %d, input %02x, ram hash %016llx\n", f, getInputStateForFrame(f), CityHash64((const char*)RAM, 0x800));
    }
  }

  vector<uint8>& preState = (frame==0) ? gEmuInitialState : gEmuStateTimeline[frame-1];
  Emulator::LoadUncompressed(&preState);
  Emulator::Step(getInputStateForFrame(frame), &EMU_GFX, &EMU_SOUND, &EMU_SSIZE, 0);
  //printf("real stepped into frame %d, input %02x, ram hash %016llx\n", frame, getInputStateForFrame(frame), CityHash64((const char*)RAM, 0x800));
}

void cmd_initAStar(mg_connection* conn, char* data, int data_len) {
  char buf[32];
  strncpy(buf, data, data_len);
  buf[data_len]=0;
  int frame = atoi(buf);
  emulateToFrame(frame);

  printf("init A* search from frame %d\n", frame);
  gAStar = boost::shared_ptr<AStar>(new AStar());
  gAStar->initFromCurrentEmulatorState();
}
void cmd_workAStar(mg_connection* conn, char* data, int data_len) {
  uint64_t st = microTime();
  int num = 100;
  for(int i=0; i<num; i++) {
    gAStar->work1();
  }
  uint64_t et = microTime();
  printf("astar work %lldms/loop\n", ((et-st)/num)/1000);
  AStar::Status s = gAStar->getStatus();

  static char buf[256 * 1024];
  char* c = buf;
  c += sprintf(c, "{\"t\":\"aStarStatus\",\"openNodes\":%lld,\"closedNodes\":%lld,\"bestPath\":[", s.openNodes, s.closedNodes);
  for(int i=0; i<s.pathToBestNode.size(); i++) {
    if(i>0) *c++ = ',';
    c += sprintf(c, "%d", s.pathToBestNode[i]);
  }
  c += sprintf(c, "]}");
  send_txt(gConn, buf);
}

void cmd_getFrame(mg_connection* conn, char* data, int data_len) {
  if(gEmuInitialState.empty()) {
    return;
  }

  static char buf[256 * 1024];
  strncpy(buf, data, data_len);
  buf[data_len]=0;
  int frame = atoi(buf);

  emulateToFrame(frame);

  send_bin(gConn, "binFrameRam", RAM, 0x800);

  static vector<uint32> palette;
  Emulator::GetPalette(palette);
  static uint32 raw[WIDTH*HEIGHT];
  for(int y=0; y<HEIGHT; y++) {
    for(int x=0; x<WIDTH; x++) {
      raw[y*WIDTH+x] = palette[EMU_GFX[y*WIDTH+x]];
    }
  }
  send_bin(gConn, "binFrameGfx", (uint8*)raw, WIDTH*HEIGHT*4);

  LuaScriptResult scriptResult;
  runLuaAgainstCurrentEmulatorState(scriptResult);
  send_bin(gConn, "binFrameLuaOutput", (uint8*)scriptResult.output.c_str(), scriptResult.output.size());

  sprintf(buf, "{\"t\":\"ramHash\",\"ramHash\":\"%016llx\",\"aStarH\":%f}", getRamHashForCurrentEmulatorState(), scriptResult.aStarH);
  send_txt(gConn, buf);

  map<InputState, NextNodeInfo> nextNodeInfos;
  getDistinctNextNodesFromCurrentEmulatorState(nextNodeInfos);
  char* c = buf;
  c += sprintf(c, "{\"t\":\"nextNodeInfos\",\"nextNodes\":{");
  map<InputState, NextNodeInfo>::const_iterator i = nextNodeInfos.begin();
  while(i != nextNodeInfos.end()) {
    if(i != nextNodeInfos.begin()) *c++ = ',';
    c+= sprintf(c, "\"%d\":{\"ramHash\":\"%016llx\",\"aStarH\":%f}", i->first, i->second.hash, i->second.aStarH);
    ++i;
  }
  c += sprintf(c, "}}");
  send_txt(gConn, buf);
}
