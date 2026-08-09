#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#endif

#include <romx/romx.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <pthread.h>
#include <unistd.h>
#endif

static int failures;
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #v); ++failures; } } while (0)

static int write_bytes(const char*path,const void*bytes,size_t size){FILE*f=fopen(path,"wb");int ok=f!=NULL&&fwrite(bytes,1U,size,f)==size&&fclose(f)==0;if(f!=NULL&&!ok)fclose(f);return ok;}

#if !defined(_WIN32)
typedef struct writer_thread { const char *output; const char *payload; const char *metadata; romx_result_t result; } writer_thread_t;
static void *write_thread(void *user){writer_thread_t*d=(writer_thread_t*)user;romx_error_t error;d->result=romx_writer_write_paths(d->output,d->payload,d->metadata,NULL,NULL,NULL,&error);return NULL;}
#endif

static void test_path_and_concurrent_writers(void)
{
#if defined(_WIN32)
    const char *directory=".";const char *payload_path="romx-phase8.gb";const char *metadata_path="romx-phase8.json";const char *output="romx-phase8.romx";
#else
    char directory[]="/tmp/libromx-phase8-XXXXXX",payload_path[512],metadata_path[512],output[512];CHECK(mkdtemp(directory)!=NULL);(void)snprintf(payload_path,sizeof(payload_path),"%s/input.gb",directory);(void)snprintf(metadata_path,sizeof(metadata_path),"%s/metadata.json",directory);(void)snprintf(output,sizeof(output),"%s/output.romx",directory);
#endif
    {
        static const char metadata[]="{\"schema_version\":\"0.1.0\",\"name\":\"Path writer\",\"platform\":\"gb\",\"payload_format\":\"gb\"}";
        romx_reader_t*reader=NULL;romx_validation_report_t report=ROMX_VALIDATION_REPORT_INIT;romx_error_t error;
        CHECK(write_bytes(payload_path,"abc",3U));CHECK(write_bytes(metadata_path,metadata,sizeof(metadata)-1U));
#if !defined(_WIN32)
        {
            pthread_t threads[8];writer_thread_t data[8];size_t i;unsigned successes=0U,exists=0U;
            for(i=0U;i<8U;++i){data[i].output=output;data[i].payload=payload_path;data[i].metadata=metadata_path;data[i].result=ROMX_E_IO;CHECK(pthread_create(&threads[i],NULL,write_thread,&data[i])==0);}
            for(i=0U;i<8U;++i){CHECK(pthread_join(threads[i],NULL)==0);if(data[i].result==ROMX_OK)++successes;else if(data[i].result==ROMX_E_EXISTS)++exists;}
            CHECK(successes==1U&&exists==7U);
        }
#else
        CHECK(romx_writer_write_paths(output,payload_path,metadata_path,NULL,NULL,NULL,&error)==ROMX_OK);
#endif
        CHECK(romx_reader_open_path(output,NULL,&reader,&error)==ROMX_OK);
        CHECK(romx_reader_validate(reader,ROMX_VALIDATE_ALL,&report,&error)==ROMX_OK);
        CHECK(report.metadata==ROMX_STATUS_VALID&&report.computed_payload_crc32==UINT32_C(0x352441c2));
        romx_reader_close(reader);
    }
    (void)remove(output);(void)remove(metadata_path);(void)remove(payload_path);
#if !defined(_WIN32)
    CHECK(rmdir(directory)==0);
#else
    (void)directory;
#endif
}

int main(void){test_path_and_concurrent_writers();if(failures)return EXIT_FAILURE;puts("all phase 8 C tests passed");return EXIT_SUCCESS;}
