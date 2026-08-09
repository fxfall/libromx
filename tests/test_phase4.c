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
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static int failures;
#define CHECK(c) do { if(!(c)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#c);++failures;} } while(0)
static void le32(uint8_t*p,uint32_t v){unsigned i;for(i=0U;i<4U;++i)p[i]=(uint8_t)(v>>(i*8U));}
static void le64(uint8_t*p,uint64_t v){unsigned i;for(i=0U;i<8U;++i)p[i]=(uint8_t)(v>>(i*8U));}

static int write_container(const char *path,int corrupt)
{
    static const char metadata[]="{\"schema_version\":\"0.1.0\",\"name\":\"A\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"crc32\":\"352441c2\"}";
    uint8_t footer[128]={0};FILE*f=fopen(path,"wb");if(!f)return 0;
    memcpy(footer,"ROMX",4U);le32(footer+4U,1U);le64(footer+8U,0U);le64(footer+16U,3U);le64(footer+24U,3U);le64(footer+32U,sizeof(metadata)-1U);le32(footer+0x58U,ROMX_FLAG_HAS_METADATA|(corrupt?ROMX_FLAG_HAS_BODY_SHA256:0U));le32(footer+0x5cU,128U);
    if(fwrite(corrupt?"axc":"abc",1U,3U,f)!=3U||fwrite(metadata,1U,sizeof(metadata)-1U,f)!=sizeof(metadata)-1U||fwrite(footer,1U,sizeof(footer),f)!=sizeof(footer)){fclose(f);return 0;}return fclose(f)==0;
}

static int file_is_abc(const char *path){char b[4]={0};FILE*f=fopen(path,"rb");size_t n;if(!f)return 0;n=fread(b,1U,sizeof(b),f);fclose(f);return n==3U&&memcmp(b,"abc",3U)==0;}

#if !defined(_WIN32)
typedef struct thread_data { const romx_reader_t *reader; const char *cache; int result; char path[1024]; } thread_data_t;
static void *cache_thread(void *user){thread_data_t*d=(thread_data_t*)user;uint64_t required=0U;romx_error_t error;d->result=(int)romx_extract_payload_cache(d->reader,d->cache,NULL,d->path,sizeof(d->path),&required,&error);return NULL;}
#endif

static void test_extraction(void)
{
#if defined(_WIN32)
    /* Windows path semantics are exercised by CI on Windows. */
    return;
#else
    char root[]="/tmp/libromx-phase4-XXXXXX";char container[512],output[512],bad_container[512],bad_output[512],cache[512];
    romx_reader_t*reader=NULL,*bad_reader=NULL;romx_error_t error;romx_extract_options_t replace=ROMX_EXTRACT_OPTIONS_INIT;
    char cache_path[1024];uint64_t required=0U;size_t i;pthread_t threads[8];thread_data_t data[8];
    CHECK(mkdtemp(root)!=NULL);(void)snprintf(container,sizeof(container),"%s/input.any",root);(void)snprintf(output,sizeof(output),"%s/output.gb",root);(void)snprintf(bad_container,sizeof(bad_container),"%s/bad.any",root);(void)snprintf(bad_output,sizeof(bad_output),"%s/bad.gb",root);(void)snprintf(cache,sizeof(cache),"%s/cache",root);
    CHECK(write_container(container,0));CHECK(write_container(bad_container,1));
    CHECK(romx_reader_open_path(container,NULL,&reader,&error)==ROMX_OK);
    CHECK(romx_extract_payload_path(reader,output,NULL,&error)==ROMX_OK);CHECK(file_is_abc(output));
    CHECK(romx_extract_payload_path(reader,output,NULL,&error)==ROMX_E_EXISTS);
    replace.flags=ROMX_EXTRACT_REPLACE_EXISTING;CHECK(romx_extract_payload_path(reader,output,&replace,&error)==ROMX_OK);CHECK(file_is_abc(output));
    CHECK(romx_extract_payload_cache(reader,cache,NULL,NULL,0U,&required,&error)==ROMX_E_BUFFER_TOO_SMALL);CHECK(required>65U);
    CHECK(romx_extract_payload_cache(reader,cache,NULL,cache_path,sizeof(cache_path),&required,&error)==ROMX_OK);CHECK(file_is_abc(cache_path));CHECK(strstr(cache_path,".gb")!=NULL);
    for(i=0U;i<8U;++i){data[i].reader=reader;data[i].cache=cache;data[i].result=-1;CHECK(pthread_create(&threads[i],NULL,cache_thread,&data[i])==0);}
    for(i=0U;i<8U;++i){CHECK(pthread_join(threads[i],NULL)==0);CHECK(data[i].result==ROMX_OK);CHECK(strcmp(data[i].path,cache_path)==0);}
    CHECK(romx_reader_open_path(bad_container,NULL,&bad_reader,&error)==ROMX_OK);CHECK(romx_extract_payload_path(bad_reader,bad_output,NULL,&error)==ROMX_E_BODY_HASH);CHECK(access(bad_output,F_OK)!=0);
    romx_reader_close(bad_reader);romx_reader_close(reader);
    CHECK(unlink(container)==0);CHECK(unlink(bad_container)==0);CHECK(unlink(output)==0);CHECK(unlink(cache_path)==0);CHECK(rmdir(cache)==0);CHECK(rmdir(root)==0);
#endif
}

int main(void){test_extraction();if(failures)return EXIT_FAILURE;puts("all phase 4 tests passed");return EXIT_SUCCESS;}
