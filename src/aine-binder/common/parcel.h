// AINE: src/aine-binder/common/parcel.h
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *data;
    size_t   size;
    size_t   capacity;
    size_t   pos;
    int      owned;
} Parcel;

void parcel_init(Parcel *p);
void parcel_init_from(Parcel *p, const void *data, size_t size);
void parcel_free(Parcel *p);
void parcel_rewind(Parcel *p);

size_t      parcel_size(const Parcel *p);
const void *parcel_data(const Parcel *p);

int parcel_write_int32(Parcel *p, int32_t v);
int parcel_write_string16(Parcel *p, const char *utf8);
int parcel_write_bytes(Parcel *p, const void *data, size_t size);
int parcel_write_interface_token(Parcel *p, const char *token);

int parcel_read_int32(Parcel *p, int32_t *v);
int parcel_read_string16(Parcel *p, char *out, size_t out_size);
int parcel_read_bytes(Parcel *p, void *out, size_t *size);
int parcel_skip_interface_token(Parcel *p);

#ifdef __cplusplus
}
#endif
