/*
 * =====================================================================================
 *
 *       Filename:  consumer_ack.h
 *
 *    Description:  ONS consumer ACK wrapper for node.js
 *
 *        Version:  1.0
 *        Created:  2016/01/25 11时16分17秒
 *       Revision:  none
 *       Compiler:  g++
 *
 *         Author:  XadillaX (ZKD), zhukaidi@souche.com
 *   Organization:  Design & Development Center, Souche Car Service Co., Ltd, HANGZHOU
 *
 * =====================================================================================
 */
#ifndef __ONS_CONSUMER_ACK_H__
#define __ONS_CONSUMER_ACK_H__
#include <uv.h>
#include <nan.h>
#include <string>
#include <cstdlib>
#include "Action.h"
using namespace std;

extern std::string ack_env_v;

class ONSListenerV8;
class ONSConsumerACKInner {
public:
    ONSConsumerACKInner(const char* msg_id) :
        acked(false),
        msg_id(msg_id)
    {
        uv_cond_init(&cond);
        uv_mutex_init(&mutex);
    }

    ~ONSConsumerACKInner()
    {
        uv_mutex_destroy(&mutex);
        uv_cond_destroy(&cond);
    }

public:
    void Ack(Action result = Action::CommitMessage)
    {
        uv_mutex_lock(&mutex);
        bool _acked = acked;

        // if acknowledged, DONOT acknowledge again
        if(_acked)
        {
            uv_mutex_unlock(&mutex);
            return;
        }

        ack_result = result;

        // write down some debug information
        // while `NODE_ONS_LOG=true`
        if(ack_env_v == "true")
        {
            printf("[%s][-----] ack: 0x%lX\n", msg_id.c_str(), (unsigned long)this);
        }

        // tell `this->WaitResult()` to continue
        acked = true;
        uv_cond_signal(&cond);
        uv_mutex_unlock(&mutex);
    }

    Action WaitResult()
    { 
        uv_mutex_lock(&mutex);

        // If `cond signal` sent before `WaitResult()`,
        // `uv_cond_wait` will blocked and will never continue
        //
        // So we have to return result directly without `uv_cond_wait`
        if(acked)
        {
            Action result = result;
            uv_mutex_unlock(&mutex);

            return result;
        }

        // Wait for `this->Ack`
        //
        // and it will emit `uv_cond_signal` to let it stop wait
        uv_cond_wait(&cond, &mutex);

        Action result = ack_result;
        uv_mutex_unlock(&mutex);

        // write down some debug information
        // while `NODE_ONS_LOG=true`
        if(ack_env_v == "true")
        {
            printf("[%s][-----] finish wait: 0x%lX\n", msg_id.c_str(), (unsigned long)this);
        }

        return result;
    }


private:
    uv_mutex_t mutex;
    uv_cond_t cond;
    Action ack_result;
    bool acked;

public:
    string msg_id;
};

class ONSConsumerACKV8 : public Nan::ObjectWrap {
public:
    friend class ONSConsumerV8;

private:
    explicit ONSConsumerACKV8();
    ~ONSConsumerACKV8();

    static NAN_METHOD(New);
    static NAN_METHOD(Done);

    static Nan::Persistent<v8::Function> constructor;

public:
    static void Init();

    static Nan::Persistent<v8::Function>& GetConstructor()
    {
        return constructor;
    }

    void SetInner(ONSConsumerACKInner* _inner)
    {
        // set the real `Acker` in the main loop
        //
        // it's thread-safe
        inner = _inner;

        if(msg_id)
        {
            delete []msg_id;
            msg_id = NULL;
        }

        msg_id = new char[inner->msg_id.size() + 1];
        strcpy(msg_id, inner->msg_id.c_str());
    }

    void Ack(Action result = Action::CommitMessage)
    {
        if(inner)
        {
            // call true `inner->Ack` in the main loop
            //
            // because this function <ONSConsumerACKV8::Ack()> will called on
            // the main loop only
            inner->Ack(result);

            // write down some debug information
            // while `NODE_ONS_LOG=true`
            if(ack_env_v == "true")
            {
                printf("[%s][---] inner unrefed: 0x%lX\n", msg_id, (unsigned long)inner);
            }

            inner = NULL;
        }
    }

private:
    ONSConsumerACKInner* inner;
    char* msg_id;
};
#endif
