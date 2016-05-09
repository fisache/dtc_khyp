#include "dtc.h"
#include "srcpos.h"

int ad_cell_count = 0, si_cell_count = 0;
int parent_ad_cell_count = 0, parent_si_cell_count = 0;

static void get_treename(char* name)
{
	strtok(name,"@");
}

static unsigned int write_propval_cells_kh(FILE *f, struct data val, int count)
{
	cell_t *cp = (cell_t *)val.val;

	if(val.len == 0) {
		return -1;
	}
	return fdt32_to_cpu(*(cp+count));
}

static unsigned int round_up(unsigned int value)
{
    unsigned int reminder = value % 0x1000;
    if (reminder) {
        return value + (0x1000 - reminder);
    }
    return value;
}

static void write_tree_source_node_kh(FILE *f, struct node *tree)
{
	struct property *prop;
	struct node *child;

	if (tree->name && (*tree->name)) {
		get_treename(tree->name);
	}

	for_each_property(tree, prop) {
		if (strncmp(prop->name, "reg\0", 4) == 0) {
			int i = 0;

			for(i = 0; i < (prop->val.len) / 4; i++) {
				unsigned int value = write_propval_cells_kh(f, prop->val, i);
                unsigned int base, offset;

                if (tree->parent->size_cells == 0) {
                    break;
                }

                if (value == 0) {
                    continue;
                }

				if (i % (tree->parent->addr_cells + tree->parent->size_cells) < tree->parent->addr_cells) {
                    base = value;
					ad_cell_count++;
				} else {
                    offset = round_up(value);
                    si_cell_count++;
				}

                if (ad_cell_count == si_cell_count) {
                    if (strncmp(tree->name, "memory", 6) == 0) {
                        fprintf(f, "\t{\"%s_%d\", 0x%x, 0x%x, 0x%x, MEMATTR_NORMAL_WB_CACHEABLE, 1},\n", tree->name, ad_cell_count, base, base, offset);
                    } else if (strncmp(tree->name, "interrupt-controller", 20) == 0 && prop->val.len == 16 * (tree->parent->addr_cells + tree->parent->size_cells)) {
                        if (i == tree->parent->addr_cells + tree->parent->size_cells - 1) {
                            fprintf(f, "\t{\"%s_%d\", 0x%x, 0x%x, 0x%x, MEMATTR_DEVICE_MEMORY, 0},\n", tree->name, ad_cell_count, base, base, offset);
                        } else if (i == 4*(tree->parent->addr_cells + tree->parent->size_cells) - 1) {
                            fprintf(f, "\t{\"%s_%d\", 0x%x, 0x%x, 0x%x, MEMATTR_DEVICE_MEMORY, 1},\n", tree->name, ad_cell_count, base-0x4000, base, offset);
                        } else {
                        }
                    } else {
                        fprintf(f, "\t{\"%s_%d\", 0x%x, 0x%x, 0x%x, MEMATTR_DEVICE_MEMORY, 1},\n", tree->name, ad_cell_count, base, base, offset);
                    }
                }
			}
		} else {
			//nothing
		}
	}

	char* child_name = NULL;

    for_each_child(tree, child) {
        if (child_name != NULL) {
            if (!strncmp(child_name, child->name, strlen(child_name)) == 0) {
                ad_cell_count = 0;
                si_cell_count = 0;
            }
        } else {
            parent_ad_cell_count = ad_cell_count;
            parent_si_cell_count = si_cell_count;
            ad_cell_count=0;
            si_cell_count=0;
        }
        write_tree_source_node_kh(f, child);
        child_name = child->name;
    }

    if (tree->children) {
        ad_cell_count = parent_ad_cell_count;
        si_cell_count = parent_si_cell_count;
    }
}

void dt_to_source_kh(FILE *f, struct boot_info *bi)
{
    fprintf(f, "#include <vm_map.h>\n");
    fprintf(f, "#include <size.h>\n\n");
    fprintf(f, "struct memdesc_t vm_device_md[] = {\n");

	write_tree_source_node_kh(f, bi->dt);
    fprintf(f, "\t{0, 0, 0, 0, 0, 0}\n");
    fprintf(f, "};");
}
