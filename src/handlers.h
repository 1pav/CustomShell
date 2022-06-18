#ifndef HANDLERS_H
#define HANDLERS_H

void msg_add_handler(const message_t * msg, proc_node * root);
void msg_info_handler(const message_t * msg, const proc_node * root);
void msg_remove_handler(const message_t * msg, proc_node * root);
void msg_list_handler(const message_t * msg, const proc_node * root);

#endif
