
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>

#include <time.h>

void delay(int number_of_seconds)
{
    // Converting time into milli_seconds
    int milli_seconds = 1000 * number_of_seconds;

    // Storing start time
    clock_t start_time = clock();

    // looping till required time is not achieved
    while (clock() < start_time + milli_seconds);
}

void onMessage(redisAsyncContext * c, void *reply, void * privdata) {
  int j;
  redisReply * r = reply;
  if (reply == NULL) return;

  printf("got a message of type: %i\n", r->type);

  if (r->type == REDIS_REPLY_ARRAY) {
    for (j = 0; j < r->elements; j++) {
      printf("%u) %s\n", j, r->element[j]->str);
    }
  }
}

void getCallback(redisAsyncContext *c, void *r, void *privdata) {
    redisReply *reply = r;
    if (reply == NULL) return;
    printf("argv[%s]: %s\n", (char*)privdata, reply->str);
}


int main(int argc, char ** argv) {
  signal(SIGPIPE, SIG_IGN);
  struct event_base * base = event_base_new();

  redisAsyncContext * c = redisAsyncConnect("www.intelligentscada.com", 6379);
  if (c->err) {
    printf("error: %s\n", c->errstr);
    return 1;
  }
 
  redisLibeventAttach(c, base);
  printf("Send Authentication CMD\n");
  redisAsyncCommand(c, getCallback, NULL, "auth ictadmin");
  printf("Send Subscribe\n");
  redisAsyncCommand(c, onMessage, NULL, "SUBSCRIBE test_channel");
  event_base_dispatch(base);

  return 0;
}
