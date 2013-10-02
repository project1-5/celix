/**
 *Licensed to the Apache Software Foundation (ASF) under one
 *or more contributor license agreements.  See the NOTICE file
 *distributed with this work for additional information
 *regarding copyright ownership.  The ASF licenses this file
 *to you under the Apache License, Version 2.0 (the
 *"License"); you may not use this file except in compliance
 *with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing,
 *software distributed under the License is distributed on an
 *"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 *specific language governing permissions and limitations
 *under the License.
 */
/*
 * bundle_revision.c
 *
 *  \date       Apr 12, 2011
 *  \author    	<a href="mailto:celix-dev@incubator.apache.org">Apache Celix Project Team</a>
 *  \copyright	Apache License, Version 2.0
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <apr_strings.h>
#include <apr_file_io.h>

#include "bundle_revision.h"
#include "archive.h"

struct bundleRevision {
	long revisionNr;
	char *root;
	char *location;
};

static apr_status_t bundleRevision_destroy(void *revisionP);

celix_status_t bundleRevision_create(apr_pool_t *pool, char *root, char *location, long revisionNr, char *inputFile, bundle_revision_pt *bundle_revision) {
    celix_status_t status = CELIX_SUCCESS;
	bundle_revision_pt revision = NULL;

	revision = (bundle_revision_pt) apr_pcalloc(pool, sizeof(*revision));
    if (!revision) {
    	status = CELIX_ENOMEM;
    } else {
		apr_status_t apr_status;
    	apr_pool_pre_cleanup_register(pool, revision, bundleRevision_destroy);
    	// TODO: This overwrites an existing revision, is this supposed to happen?
    	apr_status = apr_dir_make(root, APR_UREAD|APR_UWRITE|APR_UEXECUTE, pool);
        if ((apr_status != APR_SUCCESS) && (!APR_STATUS_IS_EEXIST(apr_status))) {
            status = CELIX_FILE_IO_EXCEPTION;
        } else {
            if (inputFile != NULL) {
                status = extractBundle(inputFile, root);
            } else if (strcmp(location, "inputstream:") != 0) {
            	// TODO how to handle this correctly?
            	// If location != inputstream, extract it, else ignore it and assume this is a cache entry.
                status = extractBundle(location, root);
            }

            if (status == CELIX_SUCCESS) {
                revision->revisionNr = revisionNr;
                revision->root = apr_pstrdup(pool, root);
                revision->location = apr_pstrdup(pool, location);
                *bundle_revision = revision;
            }
        }
    }

	return status;
}

apr_status_t bundleRevision_destroy(void *revisionP) {
	bundle_revision_pt revision = revisionP;
	return CELIX_SUCCESS;
}

celix_status_t bundleRevision_getNumber(bundle_revision_pt revision, long *revisionNr) {
	celix_status_t status = CELIX_SUCCESS;
    if (revision == NULL) {
        status = CELIX_ILLEGAL_ARGUMENT;
    } else {
    	*revisionNr = revision->revisionNr;
    }
	return status;
}

celix_status_t bundleRevision_getLocation(bundle_revision_pt revision, char **location) {
	celix_status_t status = CELIX_SUCCESS;
	if (revision == NULL) {
		status = CELIX_ILLEGAL_ARGUMENT;
	} else {
		*location = revision->location;
	}
	return status;
}

celix_status_t bundleRevision_getRoot(bundle_revision_pt revision, char **root) {
	celix_status_t status = CELIX_SUCCESS;
	if (revision == NULL) {
		status = CELIX_ILLEGAL_ARGUMENT;
	} else {
		*root = revision->root;
	}
	return status;
}
