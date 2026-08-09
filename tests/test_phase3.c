#include <romx/romx.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct memory_io { const uint8_t *data; uint64_t size; } memory_io_t;
static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static void le32(uint8_t *p, uint32_t v) { unsigned i; for (i=0U;i<4U;++i) p[i]=(uint8_t)(v>>(i*8U)); }
static void le64(uint8_t *p, uint64_t v) { unsigned i; for (i=0U;i<8U;++i) p[i]=(uint8_t)(v>>(i*8U)); }
static romx_result_t get_size(void *u,uint64_t *s,romx_error_t *e){(void)e;*s=((memory_io_t*)u)->size;return ROMX_OK;}
static romx_result_t read_at(void *u,uint64_t o,void *b,uint64_t s,uint64_t *r,romx_error_t *e){memory_io_t*m=(memory_io_t*)u;(void)e;if(o>m->size||s>m->size-o){*r=0U;return ROMX_OK;}memcpy(b,m->data+(size_t)o,(size_t)s);*r=s;return ROMX_OK;}

static uint8_t *make_file(const uint8_t *metadata, size_t metadata_size, uint64_t *file_size)
{
    static const uint8_t sha_abc[32]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    uint8_t *file;
    uint8_t *footer;
    *file_size=3U+(uint64_t)metadata_size+ROMX_FOOTER_SIZE_0_1_0;
    file=(uint8_t*)calloc(1U,(size_t)*file_size);
    memcpy(file,"abc",3U); memcpy(file+3U,metadata,metadata_size);
    footer=file+3U+metadata_size; memcpy(footer,"ROMX",4U); le32(footer+4U,1U);
    le64(footer+8U,0U); le64(footer+16U,3U); le64(footer+24U,3U); le64(footer+32U,metadata_size);
    memcpy(footer+0x38U,sha_abc,32U); le32(footer+0x58U,ROMX_FLAG_HAS_METADATA); le32(footer+0x5cU,128U);
    return file;
}

static romx_reader_t *open_memory(uint8_t *file,uint64_t size,const romx_reader_options_t *options,memory_io_t *memory)
{
    romx_io_t io=ROMX_IO_INIT; romx_reader_t *reader=NULL; romx_error_t error;
    memory->data=file;memory->size=size;io.user_data=memory;io.get_size=get_size;io.read_at=read_at;
    CHECK(romx_reader_open_io(&io,options,&reader,&error)==ROMX_OK);return reader;
}

static void test_valid_metadata(void)
{
    static const uint8_t json[]="{\"schema_version\":\"0.1.0\",\"name\":\"A\\u4e2d\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"crc32\":\"352441c2\",\"genre\":[\"one\",\"two\"],\"coop\":true}";
    uint64_t size; uint8_t *file=make_file(json,sizeof(json)-1U,&size); memory_io_t memory;
    romx_reader_t *reader=open_memory(file,size,NULL,&memory); romx_metadata_t *metadata=NULL;
    romx_validation_report_t report=ROMX_VALIDATION_REPORT_INIT; romx_error_t error;
    char label[16]; uint64_t required=0U; char raw[32];
    CHECK(romx_reader_validate(reader,ROMX_VALIDATE_METADATA,&report,&error)==ROMX_OK);
    CHECK(report.metadata==ROMX_STATUS_VALID); CHECK(report.metadata_crc32==ROMX_CRC32_VALID_LOOKUP);
    CHECK(romx_metadata_open(reader,&metadata,&error)==ROMX_OK);
    CHECK(romx_metadata_get_string(metadata,"name",label,sizeof(label),&required,&error)==ROMX_OK);
    CHECK(strcmp(label,"A\xE4\xB8\xAD")==0); CHECK(required==5U);
    CHECK(romx_metadata_get_value_json(metadata,"coop",raw,sizeof(raw),&required,&error)==ROMX_OK);
    CHECK(required==4U && memcmp(raw,"true",4U)==0);
    romx_metadata_close(metadata);romx_reader_close(reader);free(file);
}

static void test_crc_override_and_invalid_optional(void)
{
    static const char mismatch[]="{\"schema_version\":\"0.1.0\",\"name\":\"A\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"crc32\":\"00000000\"}";
    static const char invalid[]="{\"schema_version\":\"0.1.0\",\"name\":\"A\",}";
    const char *cases[2]={mismatch,invalid}; size_t i;
    for(i=0U;i<2U;++i){uint64_t size;uint8_t*file=make_file((const uint8_t*)cases[i],strlen(cases[i]),&size);memory_io_t memory;romx_reader_t*r=open_memory(file,size,NULL,&memory);romx_validation_report_t report=ROMX_VALIDATION_REPORT_INIT;romx_error_t error;romx_metadata_t*m=NULL;
        CHECK(romx_reader_validate(r,ROMX_VALIDATE_METADATA,&report,&error)==ROMX_OK);
        if(i==0U){CHECK(report.metadata==ROMX_STATUS_VALID);CHECK(report.metadata_crc32==ROMX_CRC32_VALID_LOOKUP);}
        else{CHECK(report.metadata==ROMX_STATUS_INVALID);CHECK(report.metadata_result==ROMX_E_METADATA_JSON);CHECK(romx_metadata_open(r,&m,&error)==ROMX_E_METADATA_JSON);}
        romx_metadata_close(m);romx_reader_close(r);free(file);}
}

static void test_schema_utf8_and_limit(void)
{
    static const char uppercase[]="{\"schema_version\":\"0.1.0\",\"name\":\"A\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"crc32\":\"ABCDEF12\"}";
    static const char duplicate[]="{\"schema_version\":\"0.1.0\",\"name\":\"A\",\"name\":\"B\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"crc32\":\"352441c2\"}";
    static const char escaped_duplicate[]="{\"schema_version\":\"0.1.0\",\"name\":\"A\",\"na\\u006de\":\"B\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"crc32\":\"352441c2\"}";
    static const char nul_key[]="{\"schema_version\\u0000ignored\":\"0.1.0\",\"name\":\"A\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"crc32\":\"352441c2\"}";
    static const char nul_enum[]="{\"schema_version\":\"0.1.0\",\"name\":\"A\",\"platform\":\"gb\\u0000ignored\",\"payload_format\":\"gb\",\"crc32\":\"352441c2\"}";
    const char *cases[5]={uppercase,duplicate,escaped_duplicate,nul_key,nul_enum};size_t i;
    for(i=0U;i<5U;++i){uint64_t size;uint8_t*file=make_file((const uint8_t*)cases[i],strlen(cases[i]),&size);memory_io_t memory;romx_reader_t*r=open_memory(file,size,NULL,&memory);romx_metadata_t*m=NULL;romx_error_t error;CHECK(romx_metadata_open(r,&m,&error)==ROMX_E_METADATA_SCHEMA);romx_reader_close(r);free(file);}
    {
        uint8_t invalid_utf8[]={ '{','"','s','c','h','e','m','a','_','v','e','r','s','i','o','n','"',':','"','1','.','0','"',',','"','l','a','b','e','l','"',':','"',0xff,'"',',','"','p','l','a','t','f','o','r','m','"',':','"','g','b','"',',','"','p','a','y','l','o','a','d','_','f','o','r','m','a','t','"',':','"','g','b','"','}' };
        uint64_t size;uint8_t*file=make_file(invalid_utf8,sizeof(invalid_utf8),&size);memory_io_t memory;romx_reader_t*r=open_memory(file,size,NULL,&memory);romx_metadata_t*m=NULL;romx_error_t error;CHECK(romx_metadata_open(r,&m,&error)==ROMX_E_METADATA_UTF8);romx_reader_close(r);free(file);
    }
    {
        static const char minimal[]="{\"schema_version\":\"0.1.0\",\"name\":\"A\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"crc32\":\"352441c2\"}";
        uint64_t size;uint8_t*file=make_file((const uint8_t*)minimal,strlen(minimal),&size);memory_io_t memory;romx_reader_options_t options=ROMX_READER_OPTIONS_INIT;romx_reader_t*r;romx_metadata_t*m=NULL;romx_error_t error;options.max_metadata_size=8U;r=open_memory(file,size,&options,&memory);CHECK(romx_metadata_open(r,&m,&error)==ROMX_E_METADATA_TOO_LARGE);romx_reader_close(r);free(file);
    }
}

static void test_cgb_header_priority(void)
{
    static const char json[]="{\"schema_version\":\"0.1.0\",\"name\":\"CGB\",\"platform\":\"gb\",\"payload_format\":\"gb\",\"crc32\":\"00000000\"}";
    const size_t rom_size=0x144U,metadata_size=sizeof(json)-1U;
    const uint64_t size=(uint64_t)rom_size+(uint64_t)metadata_size+ROMX_FOOTER_SIZE_0_1_0;
    uint8_t*file=(uint8_t*)calloc(1U,(size_t)size);uint8_t*footer;
    memory_io_t memory;romx_reader_t*reader;romx_error_t error;
    char format[8];uint64_t required=0U;
    CHECK(file!=NULL);if(file==NULL)return;
    file[0x143U]=0xc0U;memcpy(file+rom_size,json,metadata_size);
    footer=file+rom_size+metadata_size;memcpy(footer,"ROMX",4U);le32(footer+4U,1U);
    le64(footer+8U,0U);le64(footer+16U,rom_size);le64(footer+24U,rom_size);
    le64(footer+32U,metadata_size);le32(footer+0x58U,ROMX_FLAG_HAS_METADATA);
    le32(footer+0x5cU,ROMX_FOOTER_SIZE_0_1_0);
    reader=open_memory(file,size,NULL,&memory);
    CHECK(romx_reader_get_payload_format(reader,format,sizeof(format),&required,&error)==ROMX_OK);
    CHECK(strcmp(format,"gbc")==0);
    romx_reader_close(reader);free(file);
}

int main(void){test_valid_metadata();test_crc_override_and_invalid_optional();test_schema_utf8_and_limit();test_cgb_header_priority();if(failures)return EXIT_FAILURE;puts("all phase 3 tests passed");return EXIT_SUCCESS;}
