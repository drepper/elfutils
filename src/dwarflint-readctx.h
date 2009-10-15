#ifndef dwarflint_readctx_h
#define dwarflint_readctx_h

#include <stdbool.h>
#include "../libelf/libelf.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Functions and data structures related to bounds-checked
   reading.  */

struct read_ctx
{
  struct elf_file *file;
  Elf_Data *data;
  const unsigned char *ptr;
  const unsigned char *begin;
  const unsigned char *end;
};

uint32_t dwarflint_read_4ubyte_unaligned (struct elf_file *file,
					  const void *p);
uint64_t dwarflint_read_8ubyte_unaligned (struct elf_file *file,
					  const void *p);


void read_ctx_init (struct read_ctx *ctx,
			   struct elf_file *file,
			   Elf_Data *data);
bool read_ctx_init_sub (struct read_ctx *ctx,
			struct read_ctx *parent,
			const unsigned char *begin,
			const unsigned char *end);
uint64_t read_ctx_get_offset (struct read_ctx *ctx);
bool read_ctx_need_data (struct read_ctx *ctx, size_t length);
bool read_ctx_read_ubyte (struct read_ctx *ctx, unsigned char *ret);
int read_ctx_read_uleb128 (struct read_ctx *ctx, uint64_t *ret);
int read_ctx_read_sleb128 (struct read_ctx *ctx, int64_t *ret);
bool read_ctx_read_2ubyte (struct read_ctx *ctx, uint16_t *ret);
bool read_ctx_read_4ubyte (struct read_ctx *ctx, uint32_t *ret);
bool read_ctx_read_8ubyte (struct read_ctx *ctx, uint64_t *ret);
bool read_ctx_read_offset (struct read_ctx *ctx, bool dwarf64,
				  uint64_t *ret);
bool read_ctx_read_var (struct read_ctx *ctx, int width, uint64_t *ret);
const char *read_ctx_read_str (struct read_ctx *ctx);
bool read_ctx_skip (struct read_ctx *ctx, uint64_t len);
bool read_ctx_eof (struct read_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif
