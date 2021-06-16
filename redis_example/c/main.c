/*************************************************************************
    > File Name: redis-cli.c
    > Author: Tanswer_
    > Mail: 98duxm@gmail.com
    > Created Time: Thu Jun 28 15:49:43 2018
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hiredis/hiredis.h>

int main()
{
    // Create connection
    redisContext* c = redisConnect((char*)"www.intelligentscada.com", 6379);
    if(c->err){
        redisFree(c);
        return 0;
    }
    printf("connect redis-server success.\n");
    
    // Authentication
    const char* command_auth = "auth ictadmin";
    redisReply* r = (redisReply*)redisCommand(c, command_auth);
    if(r == NULL){
        redisFree(c);
        return 0;
    }
    if(!(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str, "OK") == 0)){
        printf("Failed to execute command[%s].\n", command_auth);
        freeReplyObject(r);
        redisFree(c);
        return 0;
    }
    else{
        freeReplyObject(r);
        printf("Succeed to execute command[%s].\n", command_auth);
    }

    /*
    const char* command = "publish";
    const char* channel = "settag2:system1.echo.value1";
    const char* message = "{\"channel\":\"specific.xxxchanneltoresponsexx\",\"user_id\":1,\"datetime\":\"2021-03-01 21:04:27.737153\",\"value\":0}";
    r = (redisReply*)redisCommand(c, "%s %s %s", command, channel, message);
    printf("r-str = %s\n", r->str);
    if(r->type != REDIS_REPLY_INTEGER){
        printf("Failed to execute command[%s].\n", command1);
        freeReplyObject(r);
        redisFree(c);
        return 0;
    }
    */
    const char* command = "set";
    const char* channel = "test_channel";
    const char* value = "hello";
    r = (redisReply*)redisCommand(c, "%s %s %s", command, channel, value);
    if(!(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str, "OK") == 0)){
        printf("Failed to execute command[%s].\n", command_auth);
        freeReplyObject(r);
        redisFree(c);
        return 0;
    }
    else{
        freeReplyObject(r);
        printf("Succeed to execute command[%s].\n", command_auth);
    }
    
    redisFree(c);
    return 0;
}
