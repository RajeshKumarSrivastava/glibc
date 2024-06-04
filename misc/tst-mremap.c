/* Tests mremap for the following cases
   - Test 1:mremap with flag MREMAP_MAYMOVE successfully extends a memory mapping
            aligned with pagesize
   - Test 2:mremap with flag MREMAP_MAYMOVE fails to map unaligned memory to pagesize
   - Test 3:mremap with flag MREMAP_FIXED successfully remaps old memory mapping
            to a new memory mapping. The previous mapping to the new_address is
            unmapped. Remapping to new_address is successfull
   - Test 4:mremap with flag MREMAP_DONTUNMAP successfully remaps.
            The old mapping is not unmapped
   - Test 5:mremap with flag MREMAP_DONTUNMAP and fifth argument new_addr, successfully
            remaps. Remapping to new_address is not successful
   Copyright (C) 2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.
   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <support/check.h>
#include <sys/mman.h>
#include <unistd.h>
#define SUCCESS_CASE 0
#define FAILURE_CASE 1

typedef struct test_construct {
    const char *test_case;
	char *old_address;
	char *new_address;
	size_t old_size;
	size_t new_size;
	int flags;
	void *exp_ret;
    int exp_errno;
	void (*setup) (struct test_construct *);
}mem_constr;
static int page_sz;

static void setup1 (struct test_construct *);
static void setup2 (struct test_construct *);
static void setup3 (struct test_construct *);
static void setup4 (struct test_construct *);
static void setup5 (struct test_construct *);
static void cleanup (void *, int);

struct test_construct t_data[] =
{
    {
        .test_case = "MREMAP_MAYMOVE remaps and extends a memory mapping, unmaps \
            the existing mapping",
	    .flags = MREMAP_MAYMOVE,
        .old_size = 2,
        .new_size = 4,
        .setup = setup1,
    },
	{
        .test_case = "MREMAP_MAYMOVE fails to remap unaligned memory to pagesize",
	    .flags = MREMAP_MAYMOVE,
        .old_size = 2,
        .new_size = 4,
	    .exp_ret = MAP_FAILED,
        .exp_errno = EINVAL,
	    .setup = setup2,
    },
	{
        .test_case = "MREMAP_FIXED extends and remaps existing memory mapping \
            to a new memory mapping specified by the page-aligned new_addr",
	    .flags = MREMAP_FIXED | MREMAP_MAYMOVE,
        .old_size = 2,
        .new_size = 4,
	    .setup = setup3,
    },
	{
        .test_case = "MREMAP_DONTUNMAP remaps the old mapping to a new address \
            without unmapping the existing one",
	    .flags = MREMAP_DONTUNMAP | MREMAP_MAYMOVE,
        .old_size = 2,
        .new_size = 2,
	    .setup = setup4,
    },
	{
        .test_case = "MREMAP_DONTUNMAP remaps the old mapping to a new one \
            without unmapping the existing one. The fifth argument to specify the address \
            range to map, but kernel returns a different address",
	    .flags = MREMAP_DONTUNMAP | MREMAP_MAYMOVE,
	    .old_size = 2,
        .new_size = 2,
	    .setup = setup5,
    },
};

static void
map_mem (char **p, size_t size)
{
    errno = 0;
    *p = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    if (p == MAP_FAILED)
        FAIL_EXIT1 ("MMAP failed with error %d", errno);
}

static void
cleanup (void *addr, int size)
{
    errno = 0;
    int ret = 0;
    if (addr)
	    ret = munmap (addr, size);

	if (ret == -1)
        FAIL_EXIT1 ("munmap failed with error %d", errno);
}

static void
setup1 (struct test_construct *t)
{
    void *p = NULL;
    t->old_size = page_sz * t->old_size;
    t->new_size = page_sz * t->new_size;

    int ret = posix_memalign (&p, page_sz, t->old_size);
    if (ret != ENOMEM){
        t->old_address = p;
        *(t->old_address) = 0x1;
    }
    else
        FAIL_EXIT1 ("posix_memalign(&p, page_sz, mem->old_size) Failed.");
}

static void
setup2 (struct test_construct *t)
{
    t->old_size = page_sz * t->old_size;
    t->new_size = page_sz * t->new_size;
    t->old_address = malloc (t->old_size);
    if (t->old_address == NULL)
        FAIL_EXIT1 ("malloc (t->old_size) Failed.");
}

static void
setup3 (struct test_construct *t)
{
    t->old_size = page_sz * t->old_size;
    t->new_size = page_sz * t->new_size;
    map_mem (&t->old_address, t->old_size);
    map_mem (&t->new_address, t->new_size);
    t->exp_ret = t->new_address;
    *(t->old_address) = 0x1;
	*(t->new_address) = 0x2;
}

static void
setup4 (struct test_construct *t)
{
    t->old_size = page_sz * t->old_size;
    map_mem (&t->old_address, t->old_size);
    *(t->old_address) = 0x1;
}

static void
setup5 (struct test_construct *t)
{
    t->old_size = page_sz * t->old_size;
    t->new_size = page_sz * t->new_size;
    map_mem (&t->old_address, t->old_size);
    map_mem (&t->new_address, t->new_size);
    *(t->old_address) = 0x1;
}

static int
mremap_tests (mem_constr t_data, int test_no)
{
    char* addr = NULL;
    int ret = 0;
    errno = 0;

    page_sz = getpagesize ();
    switch (test_no)
    {
        case 1:
            t_data.setup(&t_data);
            addr = mremap (t_data.old_address, t_data.old_size, t_data.new_size,
            t_data.flags);
            if (addr == MAP_FAILED && (*addr != (0x1)))
            {
                ret = 1;
            }
            cleanup (addr, t_data.new_size);
            break;
        case 2:
            t_data.setup(&t_data);
            addr = mremap (t_data.old_address, t_data.old_size, t_data.new_size,
            t_data.flags);
            if ((addr != MAP_FAILED) || (errno != t_data.exp_errno))
            {
                ret = 1;
            }
            break;
        case 3:
            t_data.setup(&t_data);
            addr = mremap (t_data.old_address, t_data.old_size, t_data.new_size,
            t_data.flags, t_data.new_address);
            if ((addr != MAP_FAILED) && (addr != t_data.exp_ret)
                && (*addr == *(t_data.old_address)))
            {
                printf ("mremap was not successful new_mem_addr = %p"
                "addr = %p", (void *)t_data.new_address, (void *)addr);
                ret = 1;
            }
            cleanup (addr, t_data.new_size);
            cleanup (t_data.old_address, t_data.old_size);
            cleanup (t_data.new_address, t_data.new_size);
            break;
        case 4:
            t_data.setup(&t_data);
            addr = mremap (t_data.old_address, t_data.old_size, t_data.new_size,
            t_data.flags);
            if ((addr != MAP_FAILED) && (*addr != (0x1)))
            {
                ret = 1;
            }
            cleanup (t_data.old_address, t_data.old_size);
            break;
        case 5:
            t_data.setup(&t_data);
            addr = mremap (t_data.old_address, t_data.old_size, t_data.new_size,
            t_data.flags, t_data.new_address);
            if ((addr != MAP_FAILED) && (*addr != (0x1)))
            {
                ret = 1;
            }
            cleanup (addr, t_data.new_size);
            cleanup (t_data.old_address, t_data.old_size);
            cleanup (t_data.new_address, t_data.new_size);
            break;
        default:
            break;
    }
    return ret;
}

static int
do_test (void)
{
    int ret = 0;

    if (mremap_tests (t_data[0], 1) != 0)
    {
      printf ("mremap test 1 %s failed", t_data[0].test_case);
      ret = 1;
    }
    if (mremap_tests (t_data[1], 2) != 0)
    {
      printf ("mremap test 2 %s failed", t_data[1].test_case);
      ret = 1;
    }
    if (mremap_tests (t_data[2], 3) != 0)
    {
      printf ("mremap test 3 %s failed", t_data[2].test_case);
      ret = 1;
    }
    if (mremap_tests (t_data[3], 4) != 0)
    {
      printf ("mremap test 4 %s failed", t_data[3].test_case);
      ret = 1;
    }
    if (mremap_tests (t_data[4], 5) != 0)
    {
      printf ("mremap test 5 %s failed", t_data[4].test_case);
      ret = 1;
    }
    return ret;
}

#include <support/test-driver.c>
