/*************************************************************************
 > File Name: main.c
 > Author: joseph.wang
 > Email: 350187947@qq.com
 > Created Time: Sat Feb  8 21:44:59 2020
************************************************************************/
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOGI(format,...)  printf(format "\n", ##__VA_ARGS__)
#define LOGE(format,...)  printf(format "\n", ##__VA_ARGS__)
#define ARRAY_SIZE(x)       (sizeof(x) / sizeof(x[0]))

typedef struct sd_info_t {
    uint8_t need_find_I, need_find_header;
    char *filename;
} sd_info_t, *sd_info_handle_t;

static void show_usage()
{
    printf("stream_detection <filename> -h is to find first header offset\n");
    printf("stream_detection <filename> -i is to find I offset\n");
}

uint8_t stream_header[] = {0x00, 0x00, 0x00, 0x01};
uint8_t I_frame_header[] = {0x00, 0x00, 0x00, 0x01, 0x65};
// 0x 67 sps 68 pps

/* return 0 for founded, return -1 for not founded */
static int find_header(sd_info_handle_t sd_info, uint8_t *buffer,
                       int buffer_valid_size, int *used_size)
{
    uint8_t *cur_pt = buffer;
    int i = 0;
    int header_size = 0;
    uint8_t *header_buf = NULL;

    header_buf = stream_header; 
    header_size = ARRAY_SIZE(stream_header);

    for (i = 0; i < buffer_valid_size - header_size - 1; i++) {
        cur_pt = buffer + i;
        /* for 0x 00 00 00 01 case */
        if (memcmp(cur_pt, header_buf, header_size) == 0) {
            if (sd_info->need_find_I) {
                /* Using sps header to replace I-frame header */
                if ((cur_pt[header_size] & 0x1f) == 0x7) {
                    *used_size = i;
                    return 0;
                }
            } else {
                *used_size = i;
                return 0;
            }
        }
        /* for 0x 00 00 01 case */
        if (memcmp(cur_pt, header_buf + 1, header_size -1) == 0) {
            if (sd_info->need_find_I) {
                /* Using sps header to replace I-frame header */
                if ((cur_pt[header_size - 1] & 0x1f) == 0x7) {
                    *used_size = i;
                    return 0;
                }
            } else {
                *used_size = i;
                return 0;
            }
        }
    }
    *used_size = i;

    return -1;
}
#define BUFFER_SIZE 1024
static int detect_stream(sd_info_handle_t sd_info)
{
    FILE *fp = NULL;
    uint8_t buffer[BUFFER_SIZE] = {0};
    int read_size = BUFFER_SIZE;
    int buffer_valid_size = 0;
    int used_size = 0;
    int left_size = 0;
    int offset = 0;
    int ret;
    uint8_t *current_pt = NULL;

    /* open a file */
    fp = fopen(sd_info->filename, "rb");
    if (!fp) {
        LOGE("file %s open failed", sd_info->filename);
        exit(EXIT_FAILURE);
    }
    current_pt = buffer;
    while (!feof(fp)) {
        /* read some buffer */
        ret = fread(current_pt, 1, read_size, fp);
        if (ret < 0) {
            LOGE("read failed ret %d %s", ret, strerror(errno));
            break;
        }
        buffer_valid_size = left_size + ret;
        /* find header and move buffer */
        ret = find_header(sd_info, buffer, buffer_valid_size, &used_size);
        if (!ret) {
            offset += used_size;
            LOGI("found header offset is %d", offset);
            break;
        }
        //LOGI("not found header in this buffer skip %d", ret);
        offset += used_size;
        current_pt = buffer + used_size;
        left_size = buffer_valid_size - used_size;
        memmove(buffer, current_pt, left_size);
        current_pt = buffer + left_size;
        read_size = BUFFER_SIZE - left_size;
    }

    fclose(fp);
    return 0;
}

int main(int argc, char *argv[])
{
    sd_info_handle_t sd_info;
    int ret;
	int opt;

    if (argc < 2) {
        LOGE("no filename input");
        show_usage();
        exit(EXIT_FAILURE);
    }

    sd_info = (sd_info_handle_t)calloc(1, sizeof(*sd_info));
    if (!sd_info) {
        LOGE("no mem to alloc sd_info");
        exit(EXIT_FAILURE);
    }

    sd_info->filename = argv[1];

	while ((opt = getopt(argc, argv, "ih")) != -1) {
		switch (opt) {
        case 'i':
            sd_info->need_find_I = 1;
            break;
        case 'h':
            sd_info->need_find_header = 1;
            break;
        default: /* '?' */
            show_usage();
            exit(EXIT_FAILURE);
		}
	}
    LOGI("filename %s need_find_I is %d, need_find_header is %d",
            sd_info->filename, sd_info->need_find_I, sd_info->need_find_header);

    if (sd_info->need_find_I && sd_info->need_find_header) {
        LOGE("-h and -i should not pass at the same time");
        exit(EXIT_FAILURE);
    }

    ret = detect_stream(sd_info);
    if (ret) {
        LOGE("error to incoke detect_stream %d", ret);
    }

    free(sd_info);

    return 0;
}

