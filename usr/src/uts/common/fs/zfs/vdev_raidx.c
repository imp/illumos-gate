/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2012 Cyril Plisko. All rights reserved.
 */

#include <sys/zfs_context.h>
#include <sys/spa.h>
#include <sys/vdev_impl.h>
#include <sys/zio.h>
#include <sys/zio_checksum.h>
#include <sys/fs/zfs.h>
#include <sys/fm/fs/zfs.h>

/*
 * Virtual device vector for RAID-X. Experimental RAID structure.
 */


static int
vdev_raidx_open(vdev_t *vd, uint64_t *asize, uint64_t *max_asize,
    uint64_t *ashift)
{
	vdev_t *cvd;
	uint64_t nparity = vd->vdev_nparity;
	int c;

	ASSERT(nparity > 0);

	vdev_open_children(vd);

	for (c = 0; c < vd->vdev_children; c++) {
		cvd = vd->vdev_child[c];

		*asize = MIN(*asize - 1, cvd->vdev_asize - 1) + 1;
		*max_asize = MIN(*max_asize - 1, cvd->vdev_max_asize - 1) + 1;
		*ashift = MAX(*ashift, cvd->vdev_ashift);
	}

	*asize *= vd->vdev_children;
	*max_asize *= vd->vdev_children;

	return (0);
}

static void
vdev_raidx_close(vdev_t *vd)
{
	int c;

	for (c = 0; c < vd->vdev_children; c++)
		vdev_close(vd->vdev_child[c]);
}

static uint64_t
vdev_raidx_asize(vdev_t *vd, uint64_t psize)
{
	uint64_t asize;
	uint64_t ashift = vd->vdev_top->vdev_ashift;
	uint64_t cols = vd->vdev_children;
	uint64_t nparity = vd->vdev_nparity;

	asize = ((psize - 1) >> ashift) + 1;
	asize += nparity * ((asize + cols - nparity - 1) / (cols - nparity));
	asize = roundup(asize, nparity + 1) << ashift;

	return (asize);
}

static int
vdev_raidx_io_start(zio_t *zio)
{
	vdev_t *vd = zio->io_vd;
	vdev_t *tvd = vd->vdev_top;
	vdev_t *cvd;

	ASSERT(vd != NULL);

	if (zio->io_type == ZIO_TYPE_WRITE) {
		return (ZIO_PIPELINE_CONTINUE);
	}

	ASSERT(zio->io_type == ZIO_TYPE_READ);

	return (ZIO_PIPELINE_CONTINUE);
}

static void
vdev_raidx_io_done(zio_t *zio)
{
	vdev_t *vd = zio->io_vd;

	ASSERT(vd != NULL);

	if (zio->io_type == ZIO_TYPE_WRITE) {
		return;
	}

	ASSERT(zio->io_type == ZIO_TYPE_READ);
}

static void
vdev_raidx_state_change(vdev_t *vd, int faulted, int degraded)
{
	if (faulted > vd->vdev_nparity)
		vdev_set_state(vd, B_FALSE, VDEV_STATE_CANT_OPEN,
		    VDEV_AUX_NO_REPLICAS);
	else if (degraded + faulted != 0)
		vdev_set_state(vd, B_FALSE, VDEV_STATE_DEGRADED, VDEV_AUX_NONE);
	else
		vdev_set_state(vd, B_FALSE, VDEV_STATE_HEALTHY, VDEV_AUX_NONE);
}

static void
vdev_raidx_hold(vdev_t *vd)
{
	ASSERT(vd != NULL);
}

static void
vdev_raidx_rele(vdev_t *vd)
{
	ASSERT(vd != NULL);
}

vdev_ops_t vdev_raidx_ops = {
	.vdev_op_open		= vdev_raidx_open,
	.vdev_op_close		= vdev_raidx_close,
	.vdev_op_asize		= vdev_raidx_asize,
	.vdev_op_io_start	= vdev_raidx_io_start,
	.vdev_op_io_done	= vdev_raidx_io_done,
	.vdev_op_state_change	= vdev_raidx_state_change,
	.vdev_op_hold		= vdev_raidx_hold,
	.vdev_op_rele		= vdev_raidx_rele,
	.vdev_op_type		= VDEV_TYPE_RAIDX,
	.vdev_op_leaf		= B_FALSE
};
