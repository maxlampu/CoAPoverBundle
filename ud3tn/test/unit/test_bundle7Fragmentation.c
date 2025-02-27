// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "testud3tn_unity.h"

#include "bundle7/fragment.h"

#include "ud3tn/bundle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct bundle *bundle;
static struct bundle *fragment1, *fragment2;


TEST_GROUP(bundle7Fragmentation);

TEST_SETUP(bundle7Fragmentation)
{
	bundle = NULL;
	fragment1 = NULL;
	fragment2 = NULL;
}

TEST_TEAR_DOWN(bundle7Fragmentation)
{
	if (bundle != NULL)
		bundle_free(bundle);

	if (fragment1 != NULL)
		bundle_free(fragment1);

	if (fragment2 != NULL)
		bundle_free(fragment2);
}

TEST(bundle7Fragmentation, fragment_bundle)
{
	bundle = bundle_init();

	TEST_ASSERT_NOT_NULL(bundle);

	bundle->protocol_version = 7;
	bundle->crc_type = BUNDLE_CRC_TYPE_32;

	bundle->destination = strdup("dtn:GS2");
	bundle->source = strdup("ipn:243.350");
	bundle->report_to = strdup("dtn:none");

	bundle->creation_timestamp_ms = 0;
	bundle->sequence_number = 0;
	bundle->lifetime_ms = 86400;

	struct bundle_block_list *prev;
	struct bundle_block_list *entry;
	struct bundle_block *block;

	// Previous node block
	block = bundle_block_create(BUNDLE_BLOCK_TYPE_PREVIOUS_NODE);
	entry = bundle_block_entry_create(block);

	bundle->blocks = entry;
	prev = entry;

	uint8_t previous_node[6] = {
		0x82, 0x01, 0x63, 0x47, 0x53, 0x34
	};

	block->number = 2;
	block->crc_type = bundle->crc_type;
	block->length = sizeof(previous_node);
	block->data = malloc(sizeof(previous_node));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, previous_node, sizeof(previous_node));

	// Hop count block
	block = bundle_block_create(BUNDLE_BLOCK_TYPE_HOP_COUNT);
	entry = bundle_block_entry_create(block);

	prev->next = entry;
	prev = entry;

	uint8_t hop_count[4] = { 0x82, 0x18, 0x1e, 0x00 };

	block->number = 3;
	block->flags |= BUNDLE_BLOCK_FLAG_MUST_BE_REPLICATED;
	block->crc_type = bundle->crc_type;
	block->length = sizeof(hop_count);
	block->data = malloc(sizeof(hop_count));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, hop_count, sizeof(hop_count));

	// Payload
	block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	entry = bundle_block_entry_create(block);

	prev->next = entry;

	// "Hello world"
	uint8_t payload[13] = {
		0x4c, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72,
		0x6c, 0x64, 0x21
	};

	block->number = 0;
	block->crc_type = bundle->crc_type;
	block->length = sizeof(payload);
	block->data = malloc(sizeof(payload));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, payload, sizeof(payload));
	bundle->payload_block = block;

	// NOTE: The calculation is pessimistic, assuming the biggest possible
	// header size (fragment offset == ADU length), thus, 47 and not 45 b.
	TEST_ASSERT_EQUAL(47, bundle_get_first_fragment_min_size(bundle));

	// Split payload:
	//
	//     13 = 7 + 6
	//
	fragment1 = bundle7_fragment_bundle(bundle, 0, 7);
	fragment2 = bundle7_fragment_bundle(bundle, 7, 6);

	TEST_ASSERT_NOT_NULL(fragment1);
	TEST_ASSERT_NOT_NULL(fragment2);
	TEST_ASSERT_NOT_EQUAL(bundle, fragment1);
	TEST_ASSERT_NOT_EQUAL(bundle, fragment2);
	TEST_ASSERT_NOT_NULL(fragment1->payload_block);
	TEST_ASSERT_NOT_NULL(fragment2->payload_block);

	// 1st Fragment
	//
	// Previous Node Block
	entry = fragment1->blocks;
	TEST_ASSERT_NOT_NULL(entry);
	TEST_ASSERT_EQUAL(BUNDLE_BLOCK_TYPE_PREVIOUS_NODE, entry->data->type);

	// Hop-Count Block
	entry = entry->next;
	TEST_ASSERT_NOT_NULL(entry);
	TEST_ASSERT_EQUAL(BUNDLE_BLOCK_TYPE_HOP_COUNT, entry->data->type);

	// Payload Block
	entry = entry->next;
	TEST_ASSERT_NOT_NULL(entry);
	TEST_ASSERT_EQUAL_PTR(fragment1->payload_block, entry->data);
	TEST_ASSERT_EQUAL(BUNDLE_BLOCK_TYPE_PAYLOAD, entry->data->type);
	TEST_ASSERT_EQUAL(7, fragment1->payload_block->length);
	TEST_ASSERT_NULL(entry->next);

	// 2nd Fragment
	//

	// Hop-Count Block
	entry = fragment2->blocks;
	TEST_ASSERT_NOT_NULL(entry);
	TEST_ASSERT_EQUAL(BUNDLE_BLOCK_TYPE_HOP_COUNT, entry->data->type);

	// Payload Block
	entry = entry->next;
	TEST_ASSERT_NOT_NULL(entry);
	TEST_ASSERT_EQUAL_PTR(fragment2->payload_block, entry->data);
	TEST_ASSERT_EQUAL(BUNDLE_BLOCK_TYPE_PAYLOAD, entry->data->type);
	TEST_ASSERT_EQUAL(6, fragment2->payload_block->length);
	TEST_ASSERT_NULL(entry->next);
}

TEST_GROUP_RUNNER(bundle7Fragmentation)
{
	RUN_TEST_CASE(bundle7Fragmentation, fragment_bundle);
}
