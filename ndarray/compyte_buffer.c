#include "compyte_buffer.h"

#include <stdint.h>
#include <stdlib.h>
#include <strings.h>

#define MUL_NO_OVERFLOW (1UL << (sizeof(size_t) * 4))

int GpuArray_empty(GpuArray *a, compyte_buffer_ops *ops, void *ctx, int flags,
		   size_t elsize, int nd, size_t *dims, ga_order ord) {
  size_t size = elsize;
  int i;

  if (ord == GA_ANY_ORDER)
    ord = GA_C_ORDER;

  if (ord != GA_C_ORDER && ord != GA_F_ORDER)
    return GA_VALUE_ERROR;

  for (i = 0; i < nd; i++) {
    size_t d = dims[i];
    if ((d >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) &&
	d > 0 && SIZE_MAX / d < size)
      return GA_VALUE_ERROR;
    size *= d;
  }
  a->ops = ops;
  a->data = a->ops->buffer_alloc(ctx, size);
  a->total_size = size;
  a->nd = nd;
  a->elsize = elsize;
  a->dimensions = calloc(nd, sizeof(size_t));
  a->strides = calloc(nd, sizeof(ssize_t));
  /* F/C distinction comes later */
  a->flags = GA_OWNDATA|GA_BEHAVED;
  if (a->dimensions == NULL || a->strides == NULL || a->data == NULL) {
    GpuArray_clear(res);
    return GA_MEMORY_ERROR;
  }
  /* Mult will not overflow since calloc succeded */
  bcopy(dims, res->dimensions, sizeof(size_t)*nd);

  size = elsize;
  /* mults will not overflow, checked on entry */
  switch (ord) {
  case GA_C_ORDER:
    for (i = nd-1; i >= 0; i--) {
      a->strides[i] = size;
      size *= a->dimensions[i];
    }
    a->flags |= GA_C_CONTIGUOUS;
    break;
  case GA_F_ORDER:
    for (i = 0; i < nd; i++) {
      a->strides[i] = size;
      size *= a->dimensions[i];
    }
    a->flags |= GA_F_CONTIGUOUS;
    break;
  default:
    assert(0); /* cannot be reached */
  }
  
  if (a->nd <= 1)
    a->flags |= GA_F_CONTIGUOUS|GA_C_CONTIGUOUS;

  return GA_NO_ERROR;
}

int GpuArray_zeros(GpuArray *a, compyte_buffer_ops *ops, void *ctx, int flags,
                   size_t elsize, int nd, size_t *dims, ga_order ord) {
  int err;
  err = GpuArray_empty(a, ops, ctx, flags, elsize, nd, dims, ord);
  if (err != GA_NO_ERROR)
    return err;
  err = a->ops->buffer_memset(a->data, 0, a->total_size);
  if (err != GA_NO_ERROR) {
    GpuArray_clear(a);
  }
  return err;
}

void GpuArray_clear(GpuArray *a) {
  if (a->data && GpuArray_OWNSDATA(a))
    a->ops->buffer_free(a->data);
  free(a->dimensions);
  free(a->strides);
  bzero(a, sizeof(*a));
}

int GpuArray_move(GpuArray *dst, GpuArray *src) {
  if (dst->ops != src->ops)
    return GA_INVALID_ERROR;
  if (dst->total_size != src->total_size)
    return GA_VALUE_ERROR;
  if (!GpuArray_ISWRITEABLE(dst))
    return GA_VALUE_ERROR;
  if (!GpuArray_ISONESEGMENT(dst) || !GpuArray_ISONESEGMENT(src))
    /* XXX: need to support multi-segment copies */
    return GA_UNSUPPORTED_ERROR;
  if (GpuArray_ISFORTRAN(dst) != GpuArray_ISFORTRAN(src))
    /* XXX: will need to support this too */
    return GA_UNSUPPORTED_ERROR;
  if (GpuArray_ITEMSIZE(dst) != GpuArray_ITEMSIZE(src))
    /* will have to perform casts here and know the real dtype */
    return GA_UNSUPPORTED_ERROR;
  return dst->ops->buffer_move(dst->data, src->data, dst->total_size);
}