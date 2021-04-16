#ifndef STREAM_H_
#define STREAM_H_

struct stream_t{
    char buffer[5];
    int post_pos;
    int get_pos;
};

#endif