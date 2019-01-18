#include "utils.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>

#define MAP_KEY(base, len) (map_key_t) {.key_base = base, .key_len = len}
#define MAP_VAL(base, len) (map_val_t) {.val_base = base, .val_len = len}
#define MAP_NODE(key_arg, val_arg, tombstone_arg) (map_node_t) {.key = key_arg, .val = val_arg, .tombstone = tombstone_arg}

hashmap_t *create_map(uint32_t capacity, hash_func_f hash_function, destructor_f destroy_function) {
	if(capacity == 0 || hash_function == NULL || destroy_function == NULL) {
		errno = EINVAL;
		return NULL;
	}
	hashmap_t* map = calloc(1, sizeof(hashmap_t));
	map->capacity = capacity;
	map->size = 0;
	map->hash_function = hash_function;
	map->destroy_function = destroy_function;
	map->num_readers = 0;
	map->nodes = calloc(capacity, sizeof(map_node_t));

	pthread_mutex_init(&map->write_lock, NULL);
	pthread_mutex_init(&map->fields_lock, NULL);
	map->invalid = false;
    return map;
}

bool put(hashmap_t *self, map_key_t key, map_val_t val, bool force) {
	if(self == NULL || key.key_base == NULL || key.key_len <= 0
			|| val.val_base == NULL || val.val_len <= 0) {
		errno = EINVAL;
		return false;
	}
	pthread_mutex_lock(&self->write_lock);
	if(self->invalid ) {
		errno = EINVAL;
		goto final;
	}
	if(self->size == self->capacity) {
		if(!force) {
			errno = ENOMEM;
		}
		else {
			map_node_t* node = self->nodes + get_index(self, key);
			node->val = val;
			node->key = key;
		}
		goto final;
	}
	int index = get_index(self, key);
	for(int i = 0; i < self->capacity; i++) {
		map_node_t* node = self->nodes + ((index + i) % self->capacity);
		if(node->tombstone || node->val.val_base == NULL) {
			node->val = val;
			node->key = key;
			node->tombstone = false;
			self->size++;
			goto final;
		}
	}
final:
	pthread_mutex_unlock(&self->write_lock);
    return (errno == 0);
}

static bool isCorrectKey(map_key_t req, map_key_t exist) {
	if(req.key_len == exist.key_len) {
		return (memcmp(req.key_base, exist.key_base, req.key_len) == 0);
	}
	else
		return false;
}

static void unlockWriteInRead(hashmap_t *self) {
	pthread_mutex_lock(&self->fields_lock);
	self->num_readers--;
	if(self->num_readers == 0)
		pthread_mutex_unlock(&self->write_lock);
	pthread_mutex_unlock(&self->fields_lock);
	return;
}

static int lockWriteInRead(hashmap_t *self) {
	pthread_mutex_lock(&self->fields_lock);
	if(self->invalid || self->size == 0) {
		pthread_mutex_unlock(&self->fields_lock);
		return -1;
	}
	self->num_readers++;
	if(self->num_readers == 1)
		pthread_mutex_lock(&self->write_lock);
	pthread_mutex_unlock(&self->fields_lock);
	return 0;
}

map_val_t get(hashmap_t *self, map_key_t key) {
	if(self == NULL || key.key_base == NULL || key.key_len <= 0)
		goto final;
	if(lockWriteInRead(self) < 0)
		goto final;
	int index = get_index(self, key);
	for(int i = 0; i < self->capacity; i++) {
		map_node_t* node = self->nodes + ((index + i) % self->capacity);
		if(!node->tombstone) {
			if(isCorrectKey(key, node->key)) {
				unlockWriteInRead(self);
    			return node->val;
			}
		}
	}
	unlockWriteInRead(self);
final:
	errno = EINVAL;
	return MAP_VAL(NULL,0);
}

map_node_t delete(hashmap_t *self, map_key_t key) {
	if(self == NULL || key.key_base == NULL || key.key_len <= 0)
		goto final;
	pthread_mutex_lock(&self->write_lock);
	if(self->invalid || self->size == 0) {
		pthread_mutex_unlock(&self->write_lock);
		goto final;
	}
	int index = get_index(self, key);
	for(int i = 0; i < self->capacity; i++) {
		map_node_t* node = self->nodes + ((index + i) % self->capacity);
		if(!node->tombstone && isCorrectKey(key, node->key)) {
			node->tombstone = true;
			self->size--;
			pthread_mutex_unlock(&self->write_lock);
			return *node;
		}
	}
	pthread_mutex_unlock(&self->write_lock);
final:
	errno = EINVAL;
    return MAP_NODE(MAP_KEY(NULL, 0), MAP_VAL(NULL, 0), false);
}

bool clear_map(hashmap_t *self) {
	if(self == NULL) {
		goto error;}
	pthread_mutex_lock(&self->write_lock);
	if(self->invalid) {
		pthread_mutex_unlock(&self->write_lock);
		goto error;
	}
	for(int i = 0; i < self->capacity; i++) {
		map_node_t* node = self->nodes + i;
		if(!node->tombstone) {
			if(node->key.key_base != NULL && node->val.val_base != NULL) {
				self->destroy_function(node->key, node->val);
			}
			node->key.key_len = 0;
			node->val.val_len = 0;
			node->tombstone = true;
		}
	}
	self->size = 0;
	pthread_mutex_unlock(&self->write_lock);
	return true;
error:
	errno = EINVAL;
	return false;
}

bool invalidate_map(hashmap_t *self) {
	if(self == NULL) {
		goto error;
	}
	pthread_mutex_lock(&self->write_lock);
	if(self->invalid) {
		pthread_mutex_unlock(&self->write_lock);
		goto error;
	}
	if(self->size != 0) {
		for(int i = 0; i < self->capacity; i++) {
			map_node_t* node = self->nodes + i;
			if(!node->tombstone) {
				if(node->key.key_base != NULL && node->val.val_base != NULL) {
					self->destroy_function(node->key, node->val);
					node->key.key_len = 0;
					node->val.val_len = 0;
					node->tombstone = true;
				}
			}
		}
		self->size = 0;
	}
	free(self->nodes);
	self->invalid = true;
	pthread_mutex_unlock(&self->write_lock);
	return true;
error:
	errno = EINVAL;
    return false;
}
