/*
 * gst-droid
 *
 * Copyright (C) 2014 Mohammed Sameer <msameer@foolab.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>
#include "gstgralloc.h"
#include <hardware/gralloc.h>
#include <system/window.h>

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

GST_DEBUG_CATEGORY_STATIC (gralloc_debug);
#define GST_CAT_DEFAULT gralloc_debug

typedef struct
{
  GstAllocator parent;

  gralloc_module_t *gralloc;
  alloc_device_t *alloc;
  GMutex mutex;

} GstGrallocAllocator;

typedef struct
{
  GstAllocatorClass parent_class;

} GstGrallocAllocatorClass;

typedef struct
{
  GstMemory mem;

  struct ANativeWindowBuffer buff;

} GstGrallocMemory;

G_DEFINE_TYPE (GstGrallocAllocator, gralloc_mem_allocator, GST_TYPE_ALLOCATOR);

#define GST_TYPE_GRALLOC_ALLOCATOR    (gralloc_mem_allocator_get_type())
#define GST_IS_GRALLOC_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_GRALLOC_ALLOCATOR))
#define GST_GRALLOC_ALLOCATOR(obj)    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GRALLOC_ALLOCATOR,GstGrallocAllocator))

static gboolean gst_gralloc_mem_is_span (GstMemory * mem1, GstMemory * mem2, gsize * offset);
static GstMemory * gst_gralloc_mem_copy (GstMemory * mem, gssize offset, gssize size);

static void
incRef(struct android_native_base_t* base)
{
  struct ANativeWindowBuffer *self = container_of(base, struct ANativeWindowBuffer, common);
  GstGrallocMemory *mem = container_of(self, GstGrallocMemory, buff);

  g_object_ref (mem);
}

static void
decRef(struct android_native_base_t* base)
{
  struct ANativeWindowBuffer *self = container_of(base, struct ANativeWindowBuffer, common);
  GstGrallocMemory *mem = container_of(self, GstGrallocMemory, buff);

  g_object_unref (mem);
}


GstAllocator * gst_gralloc_allocator_new (void)
{
  GST_DEBUG_CATEGORY_INIT (gralloc_debug, "gralloc", 0, "gralloc memory");

  return g_object_new (GST_TYPE_GRALLOC_ALLOCATOR, NULL);
}

static void
gralloc_mem_allocator_init (GstGrallocAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  allocator->gralloc = NULL;
  allocator->alloc = NULL;
  g_mutex_init(&allocator->mutex);
  alloc->mem_type = GST_ALLOCATOR_GRALLOC;

  alloc->mem_map = NULL;
  alloc->mem_unmap = NULL;
  alloc->mem_copy = gst_gralloc_mem_copy;
  alloc->mem_share = NULL;
  alloc->mem_is_span = gst_gralloc_mem_is_span;

  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

static void
gralloc_mem_allocator_class_init (GstGrallocAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = (GstAllocatorClass *) klass;

  allocator_class->alloc = NULL;
  // TODO: free
}

GstMemory *
gst_gralloc_allocator_alloc (GstAllocator * allocator, gint width, gint height,
			     int format, int usage)
{
  GstGrallocAllocator *alloc;
  int err;
  GstGrallocMemory *mem;
  int stride;

  if (!GST_IS_GRALLOC_ALLOCATOR (allocator)) {
    GST_WARNING ("it isn't the correct allocator for gralloc");
    return NULL;
  }

  alloc = GST_GRALLOC_ALLOCATOR (allocator);

  g_mutex_lock (&alloc->mutex);
  if (!alloc->gralloc) {
    err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t **) &alloc->gralloc);
    if (!alloc->gralloc) {
      GST_ERROR ("failed to initialize gralloc: %d", err);
      g_mutex_unlock (&alloc->mutex);
      return NULL;
    }

    err = gralloc_open((const hw_module_t *) alloc->gralloc, &alloc->alloc);
    if (err) {
      GST_ERROR ("failed to open gralloc: %d", err);
      alloc->gralloc = NULL;
      alloc->alloc = NULL;
      g_mutex_unlock (&alloc->mutex);
      return NULL;
    }
  }

  /* Now we are ready to serve */
  mem = g_slice_new0 (GstGrallocMemory);

  err = alloc->alloc->alloc(alloc->alloc, width, height,
			    format, usage, &mem->buff.handle, &stride);
  if (err) {
    GST_ERROR ("Failed to allocate buffer handle");
    g_slice_free (GstGrallocMemory, mem);
    g_mutex_unlock (&alloc->mutex);
    return NULL;
  }

  mem->buff.width = width;
  mem->buff.height = height;
  mem->buff.stride = stride;
  mem->buff.format = format;
  mem->buff.usage = usage;
  mem->buff.common.magic = ANDROID_NATIVE_BUFFER_MAGIC;
  mem->buff.common.version = sizeof(struct ANativeWindowBuffer);
  mem->buff.common.incRef = incRef;
  mem->buff.common.decRef = decRef;

  g_mutex_unlock (&alloc->mutex);

  gst_memory_init (GST_MEMORY_CAST (mem), GST_MEMORY_FLAG_NO_SHARE | GST_MEMORY_FLAG_NOT_MAPPABLE,
		   allocator, NULL, -1, -1, 0, -1);

  return GST_MEMORY_CAST (mem);
}

gboolean
gst_is_gralloc_memory (GstMemory * mem)
{
  return gst_memory_is_type (mem, GST_ALLOCATOR_GRALLOC);
}

static gboolean
gst_gralloc_mem_is_span (GstMemory * mem1, GstMemory * mem2, gsize * offset)
{
  return FALSE;
}

static GstMemory *
gst_gralloc_mem_copy (GstMemory * mem, gssize offset, gssize size)
{
  return NULL;
}
