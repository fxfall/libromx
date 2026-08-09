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
#include <unistd.h>
#endif

typedef struct memory_input { const uint8_t *bytes; uint64_t size; } memory_input_t;
static int failures;
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #v); ++failures; } } while (0)

static romx_result_t get_size(void *u,uint64_t*s,romx_error_t*e){(void)e;*s=((memory_input_t*)u)->size;return ROMX_OK;}
static romx_result_t read_at(void *u,uint64_t o,void*b,uint64_t s,uint64_t*r,romx_error_t*e){memory_input_t*i=(memory_input_t*)u;(void)e;if(o>i->size||s>i->size-o){*r=0U;return ROMX_OK;}memcpy(b,i->bytes+(size_t)o,(size_t)s);*r=s;return ROMX_OK;}
static romx_io_t make_io(memory_input_t*i){romx_io_t io=ROMX_IO_INIT;io.user_data=i;io.get_size=get_size;io.read_at=read_at;return io;}

static const uint8_t png[68]={0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x06,0x00,0x00,0x00,0x1f,0x15,0xc4,0x89,0x00,0x00,0x00,0x0b,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0x60,0x00,0x02,0x00,0x00,0x05,0x00,0x01,0x7a,0x5e,0xab,0x3f,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};

static int metadata_string(romx_reader_t*r,const char*key,const char*expected){romx_metadata_t*m=NULL;romx_error_t e;char value[32];uint64_t required=0U;int ok=romx_metadata_open(r,&m,&e)==ROMX_OK&&romx_metadata_get_string(m,key,value,sizeof(value),&required,&e)==ROMX_OK&&strcmp(value,expected)==0;romx_metadata_close(m);return ok;}

static void test_metadata_and_cover(void)
{
#if defined(_WIN32)
    const char *path="romx-phase7.romx",*override_path="romx-phase7-override.romx",*bad_path="romx-phase7-bad.romx";
#else
    char directory[]="/tmp/libromx-phase7-XXXXXX",path[512],override_path[512],bad_path[512];CHECK(mkdtemp(directory)!=NULL);(void)snprintf(path,sizeof(path),"%s/main.romx",directory);(void)snprintf(override_path,sizeof(override_path),"%s/override.romx",directory);(void)snprintf(bad_path,sizeof(bad_path),"%s/bad.romx",directory);
#endif
    {
        static const uint8_t payload_bytes[]={'a','b','c'};
        static const char template_no_crc[]="{\"schema_version\":\"0.1.0\",\"name\":\"Writer\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"cover\":{\"mime_type\":\"image/png\"}}";
        static const char template_override[]="{\"schema_version\":\"0.1.0\",\"name\":\"Writer\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"crc32\":\"AAAAAAAA\",\"origin_crc32\":\"BBBBBBBB\"}";
        memory_input_t payload={payload_bytes,3U},cover={png,sizeof(png)},broken={png,sizeof(png)};
        romx_io_t payload_io=make_io(&payload),cover_io=make_io(&cover),broken_io;
        uint8_t broken_png[sizeof(png)];romx_writer_options_t options=ROMX_WRITER_OPTIONS_INIT;
        romx_writer_report_t report=ROMX_WRITER_REPORT_INIT;romx_reader_t*reader=NULL;romx_validation_report_t validation=ROMX_VALIDATION_REPORT_INIT;romx_error_t error;
        CHECK(romx_writer_write_io_path(path,&payload_io,template_no_crc,sizeof(template_no_crc)-1U,&cover_io,NULL,&report,&error)==ROMX_OK);
        CHECK(report.metadata_size>0U&&report.cover_size==sizeof(png));
        CHECK(romx_reader_open_path(path,NULL,&reader,&error)==ROMX_OK);
        CHECK(romx_reader_validate(reader,ROMX_VALIDATE_ALL,&validation,&error)==ROMX_OK);
        CHECK(validation.metadata==ROMX_STATUS_VALID&&validation.cover==ROMX_STATUS_VALID);
        CHECK(metadata_string(reader,"crc32","352441c2"));
        romx_reader_close(reader);

        options.lookup_crc32="DEADBEEF";options.flags=ROMX_WRITER_BODY_SHA256;
        CHECK(romx_writer_write_io_path(override_path,&payload_io,template_override,sizeof(template_override)-1U,NULL,&options,&report,&error)==ROMX_OK);
        reader=NULL;CHECK(romx_reader_open_path(override_path,NULL,&reader,&error)==ROMX_OK);
        CHECK(metadata_string(reader,"crc32","deadbeef"));CHECK(metadata_string(reader,"origin_crc32","352441c2"));
        romx_reader_close(reader);

        memcpy(broken_png,png,sizeof(png));broken_png[55]^=1U;broken.bytes=broken_png;broken_io=make_io(&broken);
        CHECK(romx_writer_write_io_path(bad_path,&payload_io,template_no_crc,sizeof(template_no_crc)-1U,&broken_io,NULL,&report,&error)==ROMX_E_COVER_PNG);
        {FILE*f=fopen(bad_path,"rb");CHECK(f==NULL);if(f)fclose(f);}
        CHECK(romx_writer_write_io_path(bad_path,&payload_io,"{}",2U,NULL,NULL,&report,&error)==ROMX_E_METADATA_SCHEMA);
        {FILE*f=fopen(bad_path,"rb");CHECK(f==NULL);if(f)fclose(f);}
    }
#if defined(_WIN32)
    (void)remove(path);(void)remove(override_path);(void)remove(bad_path);
#else
    CHECK(unlink(path)==0);CHECK(unlink(override_path)==0);(void)unlink(bad_path);CHECK(rmdir(directory)==0);
#endif
}

int main(void){test_metadata_and_cover();if(failures)return EXIT_FAILURE;puts("all phase 7 tests passed");return EXIT_SUCCESS;}
