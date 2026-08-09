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
#include <sys/stat.h>
#include <unistd.h>
#endif

typedef struct memory_io{const uint8_t*data;uint64_t size;}memory_io_t;static int failures;static uint64_t fail_at=UINT64_MAX;
#define CHECK(c)do{if(!(c)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#c);++failures;}}while(0)
static void le32(uint8_t*p,uint32_t v){unsigned i;for(i=0U;i<4U;++i)p[i]=(uint8_t)(v>>(i*8U));}
static void le64(uint8_t*p,uint64_t v){unsigned i;for(i=0U;i<8U;++i)p[i]=(uint8_t)(v>>(i*8U));}
static romx_result_t gs(void*u,uint64_t*s,romx_error_t*e){(void)e;*s=((memory_io_t*)u)->size;return ROMX_OK;}
static romx_result_t ra(void*u,uint64_t o,void*b,uint64_t s,uint64_t*r,romx_error_t*e){memory_io_t*m=(memory_io_t*)u;if(o==fail_at){*r=0U;if(e){memset(e,0,sizeof(*e));e->code=ROMX_E_IO;(void)snprintf(e->message,sizeof(e->message),"injected read failure");}return ROMX_E_IO;}if(o>m->size||s>m->size-o){*r=0U;return ROMX_OK;}memcpy(b,m->data+(size_t)o,(size_t)s);*r=s;return ROMX_OK;}
static const uint8_t png_bytes[68]={0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x06,0x00,0x00,0x00,0x1f,0x15,0xc4,0x89,0x00,0x00,0x00,0x0b,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0x60,0x00,0x02,0x00,0x00,0x05,0x00,0x01,0x7a,0x5e,0xab,0x3f,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};

static uint8_t*make_file(const char*metadata,const uint8_t*png,uint64_t*size){static const uint8_t sha[32]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};size_t ms=strlen(metadata);uint8_t*f;uint8_t*footer;*size=3U+ms+sizeof(png_bytes)+128U;f=(uint8_t*)calloc(1U,(size_t)*size);memcpy(f,"abc",3U);memcpy(f+3U,metadata,ms);memcpy(f+3U+ms,png,sizeof(png_bytes));footer=f+3U+ms+sizeof(png_bytes);memcpy(footer,"ROMX",4U);le32(footer+4U,1U);le64(footer+8U,0U);le64(footer+16U,3U);le64(footer+24U,3U);le64(footer+32U,ms);le64(footer+40U,3U+ms);le64(footer+48U,sizeof(png_bytes));memcpy(footer+0x38U,sha,32U);le32(footer+0x58U,ROMX_FLAG_HAS_METADATA|ROMX_FLAG_HAS_COVER);le32(footer+0x5cU,128U);return f;}
static romx_reader_t*open_mem(uint8_t*f,uint64_t s,memory_io_t*m,const romx_reader_options_t*o){romx_io_t io=ROMX_IO_INIT;romx_reader_t*r=NULL;romx_error_t e;m->data=f;m->size=s;io.user_data=m;io.get_size=gs;io.read_at=ra;CHECK(romx_reader_open_io(&io,o,&r,&e)==ROMX_OK);return r;}

static void test_cover_valid_and_extract(void){static const char metadata[]="{\"schema_version\":\"0.1.0\",\"name\":\"A\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"crc32\":\"352441c2\",\"cover\":{\"mime_type\":\"image/png\",\"width\":1,\"height\":1}}";uint64_t s;uint8_t*f=make_file(metadata,png_bytes,&s);memory_io_t m;romx_reader_t*r=open_mem(f,s,&m,NULL);romx_validation_report_t report=ROMX_VALIDATION_REPORT_INIT;romx_cover_info_t info=ROMX_COVER_INFO_INIT;romx_error_t e;
CHECK(romx_reader_validate(r,ROMX_VALIDATE_COVER,&report,&e)==ROMX_OK);CHECK(report.cover==ROMX_STATUS_VALID);CHECK(report.cover_hashes==ROMX_STATUS_VALID);CHECK(report.cover_width==1U&&report.cover_height==1U);CHECK(romx_reader_get_cover_info(r,&info,&e)==ROMX_OK);CHECK(info.size==68U&&info.width==1U&&info.height==1U);
#if !defined(_WIN32)
{char root[]="/tmp/libromx-phase5-XXXXXX";char path[512];FILE*out;uint8_t b[68];CHECK(mkdtemp(root)!=NULL);(void)snprintf(path,sizeof(path),"%s/cover.png",root);CHECK(romx_extract_cover_path(r,path,NULL,&e)==ROMX_OK);out=fopen(path,"rb");CHECK(out!=NULL);if(out){CHECK(fread(b,1U,sizeof(b),out)==sizeof(b));CHECK(memcmp(b,png_bytes,sizeof(b))==0);fclose(out);}CHECK(unlink(path)==0);CHECK(rmdir(root)==0);}
#endif
romx_reader_close(r);free(f);}

static void test_invalid_cover_is_optional(void){static const char metadata[]="{\"schema_version\":\"0.1.0\",\"name\":\"A\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"crc32\":\"352441c2\",\"cover\":{\"mime_type\":\"image/png\"}}";uint8_t broken[68];uint64_t s;uint8_t*f;memory_io_t m;romx_reader_t*r;romx_validation_report_t report=ROMX_VALIDATION_REPORT_INIT;romx_cover_info_t info=ROMX_COVER_INFO_INIT;romx_error_t e;memcpy(broken,png_bytes,sizeof(broken));broken[55]^=1U;f=make_file(metadata,broken,&s);r=open_mem(f,s,&m,NULL);CHECK(romx_reader_validate(r,ROMX_VALIDATE_COVER,&report,&e)==ROMX_OK);CHECK(report.cover==ROMX_STATUS_INVALID);CHECK(report.cover_result==ROMX_E_COVER_PNG);CHECK(romx_reader_get_cover_info(r,&info,&e)==ROMX_E_COVER_PNG);romx_reader_close(r);free(f);}

static void test_cover_io_error_is_fatal(void){static const char metadata[]="{\"schema_version\":\"0.1.0\",\"name\":\"A\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"crc32\":\"352441c2\"}";uint64_t s;uint8_t*f=make_file(metadata,png_bytes,&s);memory_io_t m;romx_reader_t*r=open_mem(f,s,&m,NULL);romx_validation_report_t report=ROMX_VALIDATION_REPORT_INIT;romx_error_t e;fail_at=3U+(uint64_t)strlen(metadata);CHECK(romx_reader_validate(r,ROMX_VALIDATE_COVER,&report,&e)==ROMX_E_IO);CHECK(report.cover==ROMX_STATUS_NOT_CHECKED);CHECK(e.code==ROMX_E_IO);fail_at=UINT64_MAX;romx_reader_close(r);free(f);}

static void test_metadata_mismatch_and_limit(void){static const char metadata[]="{\"schema_version\":\"0.1.0\",\"name\":\"A\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"crc32\":\"352441c2\",\"cover\":{\"mime_type\":\"image/jpeg\",\"width\":2,\"sha256\":\"descriptive-only\"}}";uint64_t s;uint8_t*f=make_file(metadata,png_bytes,&s);memory_io_t m;romx_reader_t*r=open_mem(f,s,&m,NULL);romx_validation_report_t report=ROMX_VALIDATION_REPORT_INIT;romx_error_t e;CHECK(romx_reader_validate(r,ROMX_VALIDATE_COVER,&report,&e)==ROMX_OK);CHECK(report.cover==ROMX_STATUS_VALID);CHECK(report.cover_hashes==ROMX_STATUS_VALID);romx_reader_close(r);
{romx_reader_options_t o=ROMX_READER_OPTIONS_INIT;o.max_cover_size=16U;r=open_mem(f,s,&m,&o);report=(romx_validation_report_t)ROMX_VALIDATION_REPORT_INIT;CHECK(romx_reader_validate(r,ROMX_VALIDATE_COVER,&report,&e)==ROMX_OK);CHECK(report.cover==ROMX_STATUS_INVALID);romx_reader_close(r);}free(f);}

int main(void){test_cover_valid_and_extract();test_invalid_cover_is_optional();test_cover_io_error_is_fatal();test_metadata_mismatch_and_limit();if(failures)return EXIT_FAILURE;puts("all phase 5 tests passed");return EXIT_SUCCESS;}
