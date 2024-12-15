#include "binary_tree.h"
#include "list.h"
#include <stdio.h>
#include <string.h>

enum node_color_t {
    RED = 0,
    BLACK = 1
};
typedef enum node_color_t node_color;

struct binary_tree_node_t {
    binary_tree_node *parent;
    binary_tree_node *left;
    binary_tree_node *right;
    node_data data;
    node_color color;
};

struct binary_tree_t {
    binary_tree_node *root;
    comparator_t key_comparator;
    size_t key_sz;
};

[[nodiscard]] binary_tree* binary_tree_init(
    const comparator_t key_comparator,
    const size_t key_sz
) {
    binary_tree *const bt = malloc(sizeof(binary_tree));
    if (bt == NULL) {
        fprintf(stderr, "malloc NULL return in binary_tree_init\n");
        return bt;
    }
    bt->root = NULL;
    bt->key_comparator = key_comparator;
    bt->key_sz = key_sz;
    return bt;
}

[[nodiscard]] size_t binary_tree_size(const binary_tree *const this) {
    if (this == NULL || this->root == NULL) {
        return 0;
    }
    list *stack = list_init();
    list_push_back(stack, sizeof(binary_tree_node), this->root, 1);
    size_t size = 0;
    while (list_tail(stack) != NULL) {
        const binary_tree_node *const node = list_at(stack, -1)->data;
        list_pop_back(stack, 1);
        ++size;
        if (node->left != NULL) {
            list_push_back(stack, sizeof(binary_tree_node), node->left, 1);
        }
        if (node->right != NULL) {
            list_push_back(stack, sizeof(binary_tree_node), node->right, 1);
        }
    }
    list_delete(stack, 1);
    return size;
}

static binary_tree_node *binary_tree_node_init(
    binary_tree_node *restrict const parent,
    const size_t key_sz,
    void *restrict const key,
    const node_color color,
    const unsigned char intrusive
) {
    binary_tree_node *new_node = malloc(sizeof(binary_tree_node));
    if (new_node == NULL) {
        fprintf(stderr, "malloc NULL return in binary_tree_node_init\n");
        return NULL;
    }
    void *new_key;
    if (intrusive) {
        new_key = key;
    } else {
        new_key = malloc(key_sz);
        if (new_key == NULL) {
            fprintf(stderr, "malloc NULL return in binary_tree_node_init for key_sz %lu\n", key_sz);
            return NULL;
        }
        memcpy(new_key, key, key_sz);
    }
    new_node->parent = parent;
    new_node->left = NULL;
    new_node->right = NULL;
    new_node->data.data = new_key;
    new_node->data.type_sz = key_sz;
    new_node->color = color;
    return new_node;
}

static void rotate_left(binary_tree_node **root) {
    if (root == NULL) {
        return;
    }
    binary_tree_node *a = *root;
    if (a == NULL) {
        return;
    }
    binary_tree_node *parent = a->parent;
    binary_tree_node *b = a->right;
    if (b == NULL) {
        return;
    }
    binary_tree_node *c = b->left;
    b->left = a;
    a->right = c;
    if (c != NULL) {
        c->parent = a;
    }
    b->parent = parent;
    if (parent != NULL) {
        if (a == parent->left) {
            parent->left = b;
        } else {
            parent->right = b;
        }
    }
    a->parent = b;
    if (parent == NULL) {
        *root = b;
    }
}

static void rotate_right(binary_tree_node **root) {
    if (root == NULL) {
        return;
    }
    binary_tree_node *b = *root;
    if (b == NULL) {
        return;
    }
    binary_tree_node *parent = b->parent;
    binary_tree_node *a = b->left;
    if (a == NULL) {
        return;
    }
    binary_tree_node *c = a->right;
    a->right = b;
    b->left = c;
    if (c != NULL) {
        c->parent = b;
    }
    a->parent = parent;
    if (parent != NULL) {
        if (b == parent->left) {
            parent->left = a;
        } else {
            parent->right = a;
        }
    }
    b->parent = a;
    if (parent == NULL) {
        *root = a;
    }
}

void binary_tree_insert(
    binary_tree *restrict const this,
    void *restrict const key,
    const unsigned char intrusive
) {
    if (this == NULL) {
        return;
    }
    if (this->root == NULL) {
        this->root = binary_tree_node_init(NULL, this->key_sz, key, BLACK, intrusive);
        return;
    }
    binary_tree_node *parent = NULL;
    binary_tree_node *node = this->root;
    unsigned char is_left = 0;
    while (node != NULL) {
        parent = node;
        const signed char key_comparison = this->key_comparator(key, node->data.data);
        if (key_comparison < 0) { // key < node->key
            is_left = 1;
            node = node->left;
        } else if (key_comparison > 0) { // key > node->key
            is_left = 0;
            node = node->right;
        } else { // key == node->key
            // replacing element
            if (intrusive) {
                node->data.data = key;
            } else {
                memcpy(node->data.data, key, this->key_sz);
            }
            return;
        }
    }
    node = binary_tree_node_init(parent, this->key_sz, key, RED, intrusive);
    if (is_left) {
        parent->left = node;
    } else {
        parent->right = node;
    }
    if (node == NULL) {
        return;
    }
    // balancing
    binary_tree_node *grandfather = parent->parent;
    while (parent->color == RED && grandfather != NULL) {
        if (parent == grandfather->left) {
            binary_tree_node *uncle = grandfather->right;
            if (uncle != NULL && uncle->color == RED) {
                parent->color = BLACK;
                uncle->color = BLACK;
                grandfather->color = RED;
                node = grandfather;
                parent = node->parent;
                if (parent == NULL) {
                    break;
                }
                grandfather = parent->parent;
            } else {
                if (node == parent->right) {
                    node = parent;
                    rotate_left(&grandfather->left); // rotate_left(node)
                    parent = node->parent;
                    if (parent == NULL) {
                        break;
                    }
                    grandfather = parent->parent;
                    if (grandfather == NULL) {
                        break;
                    }
                }
                parent->color = BLACK;
                grandfather->color = RED;
                // rotate_right(grandfather)
                if (grandfather->parent != NULL) {
                    if (grandfather == grandfather->parent->left) {
                        rotate_right(&grandfather->parent->left);
                    } else {
                        rotate_right(&grandfather->parent->right);
                    }
                } else {
                    rotate_right(&this->root);
                }
            }
        } else {
            binary_tree_node *uncle = grandfather->left;
            if (uncle != NULL && uncle->color == RED) {
                parent->color = BLACK;
                uncle->color = BLACK;
                grandfather->color = RED;
                node = grandfather;
                parent = node->parent;
                if (parent == NULL) {
                    break;
                }
                grandfather = parent->parent;
            } else {
                if (node == parent->left) {
                    node = parent;
                    rotate_right(&grandfather->right); // rotate_right(node)
                    parent = node->parent;
                    if (parent == NULL) {
                        break;
                    }
                    grandfather = parent->parent;
                    if (grandfather == NULL) {
                        break;
                    }
                }
                parent->color = BLACK;
                grandfather->color = RED;
                // rotate_left(grandfather)
                if (grandfather->parent != NULL) {
                    if (grandfather == grandfather->parent->left) {
                        rotate_left(&grandfather->parent->left);
                    } else {
                        rotate_left(&grandfather->parent->right);
                    }
                } else {
                    rotate_left(&this->root);
                }
            }
        }
    }
    this->root->color = BLACK;
}

void binary_tree_remove(
    binary_tree *const this,
    const void *restrict const key,
    const unsigned char intrusive
) {
    if (this == NULL || this->root == NULL) {
        return;
    }
    binary_tree_node *node = this->root;
    unsigned char is_left = 0;
    for (;;) {
        const signed char cr = this->key_comparator(key, node->data.data);
        if (cr < 0) {
            node = node->left;
            if (node == NULL) {
                return;
            }
            is_left = 1;
        } else if (cr > 0) {
            node = node->right;
            if (node == NULL) {
                return;
            }
            is_left = 0;
        } else {
            break;
        }
    }
    if (!intrusive) {
        free(node->data.data);
    }
    if (node->left == NULL && node->right == NULL) {
        if (node->parent == NULL) {
            this->root = NULL;
        } else {
            if (is_left) {
                node->parent->left = NULL;
            } else {
                node->parent->right = NULL;
            }
        }
    } else if (node->left != NULL && node->right == NULL) {
        node->left->parent = node->parent;
        if (node->parent == NULL) {
            this->root = node->left;
        } else {
            if (is_left) {
                node->parent->left = node->left;
            } else {
                node->parent->right = node->left;
            }
        }
    } else if (node->left == NULL && node->right != NULL) {
        node->right->parent = node->parent;
        if (node->parent == NULL) {
            this->root = node->right;
        } else {
            if (is_left) {
                node->parent->left = node->right;
            } else {
                node->parent->right = node->right;
            }
        }
    } else {
        // replacing node with max left subtree element
        binary_tree_node *max_left = node->left;
        is_left = 1;
        while (max_left->right != NULL) {
            max_left = max_left->right;
            is_left = 0;
        }
        node->data = max_left->data;
        node = max_left;
        // replacing max left subtree element with it's own left subtree
        if (max_left->left != NULL) {
            max_left->left->parent = max_left->parent;
        }
        if (is_left) {
            max_left->parent->left = max_left->left;
        } else {
            max_left->parent->right = max_left->left;
        }
    }
    // balancing
    binary_tree_node *removed_node = node;
    while (node->color == BLACK && node->parent != NULL) {
        if (node == node->parent->left) {
            binary_tree_node *brother = node->parent->right;
            if (brother == NULL) {
                break;
            }
            if (brother->color == RED) {
                brother->color = BLACK;
                node->parent->color = RED;
                // rotate_left(parent)
                if (node->parent->parent != NULL) {
                    if (node->parent == node->parent->parent->left) {
                        rotate_left(&node->parent->parent->left);
                    } else {
                        rotate_left(&node->parent->parent->right);
                    }
                } else {
                    rotate_left(&this->root);
                }
                brother = node->parent->right;
            }
            if (
                (brother->left == NULL || brother->left->color == BLACK)
                && (brother->right == NULL || brother->right->color == BLACK)
            ) {
                brother->color = RED;
                if (node->parent->color == RED) {
                    node->parent->color = BLACK;
                    break;
                }
                node = node->parent;
            } else {
                if (brother->right == NULL || brother->right->color == BLACK) {
                    if (brother->left != NULL) {
                        brother->left->color = BLACK;
                    }
                    brother->color = RED;
                    // rotate_right(brother)
                    if (brother->parent != NULL) {
                        if (brother == brother->parent->left) {
                            rotate_right(&brother->parent->left);
                        } else {
                            rotate_right(&brother->parent->right);
                        }
                    } else {
                        rotate_right(&this->root);
                    }
                    brother = node->parent->right;
                }
                brother->color = node->parent->color;
                node->parent->color = BLACK;
                brother->right->color = BLACK;
                // rotate_left(parent)
                if (node->parent->parent != NULL) {
                    if (node->parent == node->parent->parent->left) {
                        rotate_left(&node->parent->parent->left);
                    } else {
                        rotate_left(&node->parent->parent->right);
                    }
                } else {
                    rotate_left(&this->root);
                }
                break;
            }
        } else {
            binary_tree_node *brother = node->parent->left;
            if (brother == NULL) {
                break;
            }
            if (brother->color == RED) {
                brother->color = BLACK;
                node->parent->color = RED;
                // rotate_right(parent)
                if (node->parent->parent != NULL) {
                    if (node->parent == node->parent->parent->left) {
                        rotate_right(&node->parent->parent->left);
                    } else {
                        rotate_right(&node->parent->parent->right);
                    }
                } else {
                    rotate_right(&this->root);
                }
                brother = node->parent->left;
            }
            if (
                (brother->left == NULL || brother->left->color == BLACK)
                && (brother->right == NULL || brother->right->color == BLACK)
            ) {
                brother->color = RED;
                if (node->parent->color == RED) {
                    node->parent->color = BLACK;
                    break;
                }
                node = node->parent;
            } else {
                if (brother->left == NULL || brother->left->color == BLACK) {
                    if (brother->right != NULL) {
                        brother->right->color = BLACK;
                    }
                    brother->color = RED;
                    // rotate_left(brother)
                    if (brother->parent != NULL) {
                        if (brother == brother->parent->left) {
                            rotate_left(&brother->parent->left);
                        } else {
                            rotate_left(&brother->parent->right);
                        }
                    } else {
                        rotate_left(&this->root);
                    }
                    brother = node->parent->left;
                }
                brother->color = node->parent->color;
                node->parent->color = BLACK;
                brother->left->color = BLACK;
                // rotate_right(parent)
                if (node->parent->parent != NULL) {
                    if (node->parent == node->parent->parent->left) {
                        rotate_right(&node->parent->parent->left);
                    } else {
                        rotate_right(&node->parent->parent->right);
                    }
                } else {
                    rotate_right(&this->root);
                }
                break;
            }
        }
    }
    this->root->color = BLACK;
    free(removed_node);
}

[[nodiscard]] static binary_tree_node *binary_tree_node_at(
    const binary_tree *const this,
    const void *restrict const key
) {
    if (this == NULL) {
        return NULL;
    }
    binary_tree_node *node = this->root;
    while (node != NULL) {
        const signed char cr = this->key_comparator(key, node->data.data);
        if (cr < 0) {
            node = node->left;
        } else if (cr > 0) {
            node = node->right;
        } else {
            return node;
        }
    }
    return NULL;
}

[[nodiscard]] node_data *binary_tree_at(
    const binary_tree *const this,
    const void *restrict const key
) {
    if (this == NULL) {
        return NULL;
    }
    binary_tree_node *node = binary_tree_node_at(this, key);
    if (node == NULL) {
        return NULL;
    }
    return &node->data;
}

[[maybe_unused]] static void post_order(
    binary_tree_node *const this,
    list *const lrv
) {
    if (this == NULL) {
        return;
    }
    list* stack = list_init();
    list_push_back(stack, sizeof(binary_tree_node), this, 1);
    while (list_tail(stack) != NULL) {
        binary_tree_node *const node = list_at(stack, -1)->data;
        list_pop_back(stack, 1);
        list_push_front(lrv, sizeof(binary_tree_node), node, 1);
        if (node->left != NULL) {
            list_push_back(stack, sizeof(binary_tree_node), node->left, 1);
        }
        if (node->right != NULL) {
            list_push_back(stack, sizeof(binary_tree_node), node->right, 1);
        }
    }
    list_delete(stack, 1);
}

[[maybe_unused]] static void in_order(
    binary_tree_node *const this,
    list *const lvr
) {
    if (this == NULL) {
        return;
    }
    list* stack = list_init();
    binary_tree_node *node = this;
    unsigned char left = 1;
    for (;;) {
        if (left) {
            while (node->left != NULL) {
                list_push_back(stack, sizeof(binary_tree_node), node, 1);
                node = node->left;
            }
        }
        left = 1;
        list_push_back(lvr, sizeof(binary_tree_node), node, 1);
        if (node->right != NULL) {
            node = node->right;
            continue;
        }
        if (list_tail(stack) == NULL) {
            break;
        }
        node = list_at(stack, -1)->data;
        list_pop_back(stack, 1);
        left = 0;
    }
    list_delete(stack, 1);
}

[[maybe_unused]] static void pre_order(
    binary_tree_node *const this,
    list *const vlr
) {
    if (this == NULL) {
        return;
    }
    list* stack = list_init();
    list_push_back(stack, sizeof(binary_tree_node), this, 1);
    while (list_tail(stack) != NULL) {
        binary_tree_node *const node = list_at(stack, -1)->data;
        list_push_back(vlr, sizeof(binary_tree_node), node, 1);
        list_pop_back(stack, 1);
        if (node->right != NULL) {
            list_push_back(stack, sizeof(binary_tree_node), node->right, 1);
        }
        if (node->left != NULL) {
            list_push_back(stack, sizeof(binary_tree_node), node->left, 1);
        }
    }
    list_delete(stack, 1);
}

void binary_tree_visit_range(
    const binary_tree *const restrict this,
    const void *min,
    const void *max,
    visit_t visit
) {
    if (this == NULL || this->root == NULL) {
        return;
    }
    const signed char comparison_result = this->key_comparator(min, max);
    if (comparison_result == 0) { // min == max
        binary_tree_node *node = binary_tree_node_at(this, min);
        visit(&node->data);
    }
    if (comparison_result > 0) { // min > max => swapping
        const void *const tmp = min;
        min = max;
        max = tmp;
    }
    // in_order visit
    list* stack = list_init();
    binary_tree_node *node = this->root;
    unsigned char left = 1;
    for (;;) {
        if (left) {
            while (node->left != NULL) {
                list_push_back(stack, sizeof(binary_tree_node), node, 1);
                node = node->left;
            }
        }
        left = 1;
        const signed char min_comp = this->key_comparator(node->data.data, min);
        const signed char max_comp = this->key_comparator(node->data.data, max);
        if (min_comp >= 0 && max_comp <= 0) {
            visit(&node->data);
        }
        if (node->right != NULL) {
            node = node->right;
            continue;
        }
        if (list_tail(stack) == NULL) {
            break;
        }
        node = list_at(stack, -1)->data;
        list_pop_back(stack, 1);
        left = 0;
    }
    list_delete(stack, 1);
}

void binary_tree_delete(
    binary_tree *const this,
    const unsigned char intrusive
) {
    if (this == NULL) {
        return;
    }
    if (this->root != NULL) {
        list* stack = list_init();
        binary_tree_node **node = &this->root;
        for (;;) {
            while ((*node)->left != NULL) {
                list_push_back(stack, sizeof(binary_tree_node*), node, 1);
                node = &(*node)->left;
            }
            if ((*node)->right != NULL) {
                list_push_back(stack, sizeof(binary_tree_node*), node, 1);
                node = &(*node)->right;
                continue;
            }
            if (!intrusive) {
                free((*node)->data.data);
            }
            free(*node);
            *node = NULL;
            if (list_tail(stack) == NULL) {
                break;
            }
            node = list_at(stack, -1)->data;
            list_pop_back(stack, 1);
        }
        list_delete(stack, 1);
    }
    free(this);
}
