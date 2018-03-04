#define MAX_HOPS 512

struct potato_t {
  int rest_hops;
  char trace[4*MAX_HOPS];
};
