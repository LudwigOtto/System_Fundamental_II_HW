#include "queue.h"
#include <errno.h>

queue_t *create_queue(void) {
	queue_t *queue = calloc(1, sizeof(queue_t));
	queue->invalid = false;
	sem_init(&queue->items, 0, 0);
	pthread_mutex_init(&queue->lock, NULL);
    return queue;
}

bool invalidate_queue(queue_t *self, item_destructor_f destroy_function) {
	if(self == NULL) {
		goto error;
	}
	pthread_mutex_lock(&self->lock);
	if(self->invalid) {
		pthread_mutex_unlock(&self->lock);
		goto error;
	}
	int items;
	sem_getvalue(&self->items, &items);
	while(items > 0) {
		sem_wait(&self->items);
		queue_node_t *node = self->front;
		void* item = node->item;
		if(self->front != self->rear) {
			self->front = self->front->next;
		}
		else {
			self->front = NULL;
			self->rear = NULL;
		}
		free(node);
		destroy_function(item);
		sem_getvalue(&self->items, &items);
	}
	self->invalid = true;
	pthread_mutex_unlock(&self->lock);
    return true;
error:
	errno = EINVAL;
	return false;
}

bool enqueue(queue_t *self, void *item) {
	if(self == NULL || item == NULL) {
		goto error;
	}
	pthread_mutex_lock(&self->lock);
	if(self->invalid) {
		pthread_mutex_unlock(&self->lock);
		goto error;
	}
	queue_node_t *node = calloc(1, sizeof(queue_node_t));
	node->item = item;
	node->next = NULL;
	if(self->front == NULL) {
		self->front = node;
		self->rear = node;
		self->front->next = self->rear;
	}
	else {
		self->rear->next = node;
		self->rear = node;
	}
	sem_post(&self->items);
	pthread_mutex_unlock(&self->lock);
	return true;
error:
	errno = EINVAL;
	return false;
}

void *dequeue(queue_t *self) {
	if(self == NULL) {
		goto error;
	}
	sem_wait(&self->items);
	pthread_mutex_lock(&self->lock);
	if(self->invalid) {
		pthread_mutex_unlock(&self->lock);
		goto error;
	}
	queue_node_t *node = self->front;
	void* item = node->item;
	if(self->front != self->rear) {
		self->front = self->front->next;
	}
	else {
		self->front = NULL;
		self->rear = NULL;
	}
	free(node);
	pthread_mutex_unlock(&self->lock);
    return item;
error:
	errno = EINVAL;
	return NULL;
}
