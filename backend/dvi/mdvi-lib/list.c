/*
 * Copyright (C) 2000, Matias Atria
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include "common.h"

void	listh_init(ListHead *head)
{
	head->head = head->tail = NULL;
	head->count = 0;
}

void	listh_prepend(ListHead *head, List *list)
{
	list->prev = NULL;
	list->next = head->head;
	if(head->head)
		head->head->prev = list;
	head->head = list;
	if(!head->tail)
		head->tail = list;
	head->count++;
}

void	listh_append(ListHead *head, List *list)
{
	list->next = NULL;
	list->prev = head->tail;
	if(head->tail)
		head->tail->next = list;
	else
		head->head = list;
	head->tail = list;
	head->count++;
}

void	listh_add_before(ListHead *head, List *at, List *list)
{
	if(at == head->head || head->head == NULL)
		listh_prepend(head, list);
	else {
		list->next = at;
		list->prev = at->prev;
		at->prev = list;
		head->count++;
	}
}

void	listh_add_after(ListHead *head, List *at, List *list)
{
	if(at == head->tail || !head->tail)
		listh_append(head, list);
	else {
		list->prev = at;
		list->next = at->next;
		at->next = list;
		head->count++;
	}
}

void	listh_remove(ListHead *head, List *list)
{
	if(list == head->head) {
		head->head = list->next;
		if(head->head)
			head->head->prev = NULL;
	} else if(list == head->tail) {
		head->tail = list->prev;
		if(head->tail)
			head->tail->next = NULL;
	} else {
		list->next->prev = list->prev;
		list->prev->next = list->next;
	}
	if(--head->count == 0)
		head->head = head->tail = NULL;
}

void	listh_concat(ListHead *h1, ListHead *h2)
{
	if(h2->head == NULL)
		; /* do nothing */
	else if(h1->tail == NULL)
		h1->head = h2->head;
	else {
		h1->tail->next = h2->head;
		h2->head->prev = h1->tail;
	}
	h1->tail = h2->tail;
	h1->count += h2->count;
}

void	listh_catcon(ListHead *h1, ListHead *h2)
{
	if(h2->head == NULL)
		; /* do nothing */
	else if(h1->head == NULL)
		h1->tail = h2->tail;
	else {
		h1->head->prev = h2->tail;
		h2->tail->next = h1->head;
	}
	h1->head = h2->head;
	h1->count += h2->count;
}
