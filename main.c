#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>

// https://en.wikipedia.org/wiki/Knuth's_Algorithm_X
// https://github.com/Elementrix08/Sudoku/blob/master/Dancing-Links.cpp
// https://garethrees.org/2007/06/10/zendoku-generation/#figure-2

#define CELL unsigned long

// sudoku board. top left hand corner is (0,0)
CELL *board = NULL;
unsigned int board_size = 0, board_grid_size = 0, board_cell_size = 0, board_line_size = 0;

const unsigned int BOARD_MAX_CELL_SIZE = 7;
const unsigned int BOARD_MAX_LINE_SIZE = BOARD_MAX_CELL_SIZE * BOARD_MAX_CELL_SIZE;
const unsigned int BOARD_MAX_SIZE = BOARD_MAX_LINE_SIZE * BOARD_MAX_LINE_SIZE;

// MAX_INPUT_LINE_SIZE = number of numbers per line + number of value separators + number of cell separators + newline + null byte
const unsigned int MAX_INPUT_LINE_SIZE = BOARD_MAX_LINE_SIZE + (BOARD_MAX_LINE_SIZE - BOARD_MAX_CELL_SIZE) + (BOARD_MAX_CELL_SIZE - 1) + 2;

const char BOARD_EMPTY = '.', BOARD_CSEP = ',', BOARD_VWALL = '|', BOARD_HWALL = '=', BOARD_JOIN = '#';
const char INPUT_CSEP[] = { BOARD_CSEP, '\0' };
const char INPUT_VWALL[] = { BOARD_VWALL, '\0' };
const char INPUT_HWALL[] = { BOARD_HWALL, '\0' };

// if defined, will ignore sudoku grid and construct test matrix for dlx
#define DLX_TESTING
#undef DLX_TESTING

#define DEBUG_STEP_PRINT
#undef DEBUG_STEP_PRINT

/* ===========================================================================
 * Sudoku Grid Utilities
 * ===========================================================================
 */

CELL board_get(unsigned int i, unsigned int j) {
	size_t idx = i + (j * board_line_size);

	if (board && idx < board_size) {
		return board[idx];
	}

	return 0;
}

void board_set(unsigned int i, unsigned int j, CELL value) {
	size_t idx = i + (j * board_line_size);

	if (board && idx < board_size) {
		board[idx] = value;
	}
}

bool board_try_resize(size_t size) {
	if (size) {
		board_size = size;
		board = realloc(board, size * sizeof(CELL));

		return board != NULL;
	}

	return false;
}

bool try_read_board(FILE *input) {
	static char buf[4096];
	const size_t BUF_SIZE = sizeof(buf) / sizeof(char);

	// get first line in sudoku grid
	char *line = fgets(buf, BUF_SIZE, input);

	unsigned int cell_size = 0, line_size = 0, grid_size = 0;
	char *first_cell = strtok(line, INPUT_VWALL);
	size_t first_cell_len = strlen(first_cell);

	if (first_cell) {
		// count the number of cell value separators in the first cell
		for (size_t i = 0; i < first_cell_len; i++) {
			cell_size += first_cell[i] == BOARD_CSEP;
		}
		cell_size++;

		line_size = cell_size * cell_size;
		grid_size = line_size * line_size;
	}

	board_cell_size = cell_size;
	board_line_size = line_size;
	board_grid_size = grid_size;

	if (!board_try_resize(grid_size)) {
		printf("Failed to resize board to required amount: %u!\n", grid_size);
		return false;
	}

	if (fseek(input, 0, SEEK_SET)) {
		printf("Failed to seek to start of file!\n");
		return false;
	}

	for (unsigned int i = 0, j = 0; j < line_size; j++) {
		line = fgets(buf, BUF_SIZE, input);

		// ignore horizontal cell separator lines
		if (line[0] == BOARD_HWALL) { j--; continue; }

		char *cell_saveptr = NULL, *value_saveptr = NULL;

		char *cell = strtok_r(line, INPUT_VWALL, &cell_saveptr);
		while (cell) {
			char *value = strtok_r(cell, INPUT_CSEP, &value_saveptr);
			while (value) {
				CELL parsed = strtoul(value, NULL, 10);

				if (parsed) {
					board_set(i++, j, parsed);
				} else {
					board_set(i++, j, 0);
				}

				value = strtok_r(NULL, INPUT_CSEP, &value_saveptr);
			}

			cell = strtok_r(NULL, INPUT_VWALL, &cell_saveptr);
		}

		i = 0;
		memset(buf, 0, strlen(line));
	}

	return true;
}

void board_print() {
	char value_buf[3] = { 0 };

	for (unsigned int j = 0; j < board_line_size; j++) {
		if (j && (j % board_cell_size) == 0) {
			for (unsigned int k = 0; k < board_line_size; k++) {
				if (k && (k % board_cell_size) == 0) {
					printf("%c%c", BOARD_JOIN, BOARD_HWALL);
				}

				const char BOARD_HSEP[] = {
					BOARD_HWALL, BOARD_HWALL, BOARD_HWALL, BOARD_HWALL, '\0'
				};
				printf("%s", BOARD_HSEP);
			}

			printf("\n");
		}

		for (unsigned int i = 0; i < board_line_size; i++) {
			CELL value = board_get(i, j);
			snprintf(value_buf, 3, "%lu", value);

			if (i && (i % board_cell_size) == 0) {
				printf("%c ", BOARD_VWALL);
			}

			printf("%3s ", value_buf);
		}

		printf("\n");
	}

	printf("\n");
}

/* ===========================================================================
 * Dancing Links + Algorithm X
 * ===========================================================================
 */

struct dlx_matrix;
struct dlx_node;

enum DLX_TYPE {
	DLX_TYPE_DATA = 0,
	DLX_TYPE_COLUMN = 1
};

struct dlx_node {
	enum DLX_TYPE type;
	struct dlx_node *left, *right, *up, *down;

	union {
		struct {
			size_t row_id;
			struct dlx_node *parent;
		} data; // matrix data node

		struct {
			size_t id;
			size_t count;
		} column; // matrix column node
	};
};

struct dlx_matrix {
	struct dlx_node *root;
	bool solved;
};

struct dlx_node *node_create(enum DLX_TYPE type, struct dlx_node *parent, size_t id) {
	struct dlx_node *node = malloc(sizeof(struct dlx_node));

	node->type = type;

	node->left = NULL;
	node->right = NULL;
	node->up = NULL;
	node->down = NULL;

	switch (type) {
		case DLX_TYPE_DATA:
			node->data.row_id = id;
			node->data.parent = parent;
			break;

		case DLX_TYPE_COLUMN:
			node->column.id = id;
			node->column.count = 0;
			break;
	}

	return node;
}

void node_free(struct dlx_node *val) {
	free(val);
}

typedef unsigned char linkmask_t;

const linkmask_t LINK_UP	= 0b00000001;
const linkmask_t LINK_RIGHT	= 0b00000010;
const linkmask_t LINK_DOWN	= 0b00000100;
const linkmask_t LINK_LEFT	= 0b00001000;

void link_nodes(struct dlx_node *a, struct dlx_node *b, linkmask_t mask) {
	if (!a || !b) return;

	if (mask & LINK_LEFT) { a->left = b; b->right = a; }
	if (mask & LINK_RIGHT) { a->right = b; b->left = a; }
	if (mask & LINK_UP) { a->up = b; b->down = a; }
	if (mask & LINK_DOWN) { a->down = b; b->up = a; }
}

// converts the board into a sparse matrix.
struct dlx_matrix *matrix_create() {
	struct dlx_matrix *mat = malloc(sizeof(struct dlx_matrix));

	// TODO(mikolaj): map sudoku into an exact cover (hitting set) problem

	// NOTE(mikolaj): temporary example dlx matrix
	mat->solved = false;

	// we need to create the header value for our columns
	struct dlx_node *h = node_create(DLX_TYPE_COLUMN, NULL, 0);
	mat->root = h;

	// the constraints will be the columns in our dlx matrix. there are 4 
	// different sets of constraints that we need to solve for:
	//   - row-column : represents the need to have a given number at the
	//                  intersection of a given row and given column
	//   - row-number : represents the need to have 1 of a given number in 
	//                  a given row
	//   - col-number : represents the need to have 1 of a given number in 
	//                  a given column
	//   - box-number : represents the need to have 1 of a given number in
	//                  a given cell (box)
	size_t constraint_set_size = board_line_size * board_line_size;
	size_t cols = 4 * constraint_set_size;

	struct dlx_node **constraints = malloc(cols * sizeof(struct dlx_node*));

	// we populate the column object row, linking all the different columns 
	// together. this linked list is made circular by the final linking back 
	// to the header
	struct dlx_node *previous_constraint = h;
	for (size_t col = 0; col < cols; col++) {
		struct dlx_node *constraint = node_create(DLX_TYPE_COLUMN, NULL, col);

		constraints[col] = constraint;

		// 2D linked lists form a torus, allowing us to later easily 
		// add data nodes to each column
		link_nodes(constraint, constraint, LINK_DOWN);
		link_nodes(previous_constraint, constraint, LINK_RIGHT);

		previous_constraint = constraint;
	}
	link_nodes(previous_constraint, h, LINK_RIGHT);

	// the possibilities will be the different placements of every number 
	// in every possible space on the board. these will be the rows in our 
	// dlx matrix
	size_t placement_set_size = board_grid_size;
	size_t number_set_size = board_line_size;
	size_t rows = number_set_size * placement_set_size;

	// we populate each row with 4 nodes, one for each of the different 
	// constraint
	for (size_t row = 0; row < rows; row++) {
		size_t row_idx = row / board_grid_size;
		size_t col_idx = (row / board_line_size) % board_line_size;
		size_t offset = row % board_line_size;

		// we only create a data object for a sudoku grid cell if the 
		// cell has a non-zero value (aka has a valid value), and if 
		// the value if equal to the current offset. this is because 
		// the grid is 2D, and the row is 1D, so we have to map from 
		// the grid to the row (this is done via the offset index)
		CELL value = board_get(col_idx, row_idx);
		if (value && value != offset + 1)
			continue;


		// to create our data elements, we need to first fetch the 
		// constraint each element will satisfy. this is done by mapping 
		// the current row_idx, col_idx, and offset into a flat index 
		// into the constraints array
		struct dlx_node *num_constraint = constraints[(0 * constraint_set_size) + (row / board_line_size)];
		struct dlx_node *row_constraint = constraints[(1 * constraint_set_size) + (row_idx * board_line_size) + offset];
		struct dlx_node *col_constraint = constraints[(2 * constraint_set_size) + (col_idx * board_line_size) + offset];
		struct dlx_node *box_constraint = constraints[(3 * constraint_set_size) + ((board_cell_size * (row_idx / board_cell_size) + (col_idx / board_cell_size)) * board_line_size) + offset];

		// we create the data elements with the correct row id and parent constraint (column header)
		struct dlx_node *num_node = node_create(DLX_TYPE_DATA, num_constraint, row);
		struct dlx_node *row_node = node_create(DLX_TYPE_DATA, row_constraint, row);
		struct dlx_node *col_node = node_create(DLX_TYPE_DATA, col_constraint, row);
		struct dlx_node *box_node = node_create(DLX_TYPE_DATA, box_constraint, row);

		// we need to keep track of the number of elements in each column
		num_constraint->column.count++;
		row_constraint->column.count++;
		col_constraint->column.count++;
		box_constraint->column.count++;

		// link nodes onto bottom nodes in columns
		link_nodes(num_constraint->up, num_node, LINK_DOWN);
		link_nodes(row_constraint->up, row_node, LINK_DOWN);
		link_nodes(col_constraint->up, col_node, LINK_DOWN);
		link_nodes(box_constraint->up, box_node, LINK_DOWN);

		// link column headers onto bottoms of nodes
		link_nodes(num_constraint, num_node, LINK_UP);
		link_nodes(row_constraint, row_node, LINK_UP);
		link_nodes(col_constraint, col_node, LINK_UP);
		link_nodes(box_constraint, box_node, LINK_UP);

		// link row across
		link_nodes(num_node, row_node, LINK_RIGHT);
		link_nodes(row_node, col_node, LINK_RIGHT);
		link_nodes(col_node, box_node, LINK_RIGHT);
		link_nodes(box_node, num_node, LINK_RIGHT);
	}

	free(constraints);

	return mat;
}

void matrix_free(struct dlx_matrix *val) {
	struct dlx_node *root = val->root;
	for (struct dlx_node *column = root->left, *next_column; column != root; column = next_column) {
		for (struct dlx_node *row = column->up, *next_row; row != column; row = next_row) {
			next_row = row->up;
			node_free(row);
		}

		next_column = column->left;
		node_free(column);
	}

	node_free(root);
	free(val);
}


void matrix_print(struct dlx_node *root) {
	struct dlx_node *curr = root;

	do {
		if (curr->type == DLX_TYPE_COLUMN) {
			printf("%lu(%lu): ", curr->column.id, curr->column.count);

			for (struct dlx_node *elem = curr->down; elem != curr; elem = elem->down) {
				printf("elem(%lu) ", elem->data.row_id);
			}

			printf("\n");
		}

		curr = curr->right;
	} while (curr && curr != root);
}

struct dlx_node *choose_min_length_column(struct dlx_matrix *matrix) {
	size_t min_count = (2 << sizeof(size_t)) - 1;

	struct dlx_node *root = matrix->root, *best = NULL;
	for (struct dlx_node *curr = root->right; curr != root; curr = curr->right) {
		if (curr->column.count < min_count) {
			best = curr;
			min_count = curr->column.count;
		}
	}

	return best;
}

void cover_column(struct dlx_node *col) {
	assert(col->type == DLX_TYPE_COLUMN);
	// cut column from column list
	col->left->right = col->right;
	col->right->left = col->left;

	// removes column's rows from other columns
	for (struct dlx_node *row = col->down; row != col; row = row->down) {
		// affect all nodes in same row
		for (struct dlx_node *node = row->right; node != row; node = node->right) {
			// cut node from linked list
			node->up->down = node->down;
			node->down->up = node->up;

			node->data.parent->column.count--;
		}
	}

}

void uncover_column(struct dlx_node *col) {
	assert(col->type == DLX_TYPE_COLUMN);
	// restores column's rows in other columns in inverse order
	for (struct dlx_node *row = col->up; row != col; row = row->up) {
		// affect all nodes in same row
		for (struct dlx_node *node = row->left; node != row; node = node->left) {
			node->data.parent->column.count++;

			// restore node to the linked list
			node->down->up = node;
			node->up->down = node;
		}
	}

	// restore column to column list
	col->right->left = col;
	col->left->right = col;
}

void step_print(unsigned int k, const char *format, ...) {
#ifdef DEBUG_STEP_PRINT
	static char buf[4096];

	size_t format_len = strlen(format);
	size_t buf_len = format_len + k;

	for (size_t i = 0, j = k; i < buf_len; i++) {
		if (j) {
			buf[i] = '\t';
			j--;
		} else {
			buf[i] = format[i - k];
		}
	}
	buf[buf_len] = '\0';

	va_list argp;
	va_start(argp, format);
	vprintf(buf, argp);
	va_end(argp);
#endif
}

bool dlx_solve_impl(struct dlx_matrix *matrix, unsigned int k, struct dlx_node ***solution, size_t *solution_len);
bool dlx_solve_impl(struct dlx_matrix *matrix, unsigned int k, struct dlx_node ***solution, size_t *solution_len) {
	struct dlx_node *root = matrix->root;

	step_print(k, "Solve(%u):\n", k);

	if (root->right == root) {
		step_print(k, "Solved!\n");
		return true; // matrix has no columns, found solution
	}

	struct dlx_node *col = choose_min_length_column(matrix);
	step_print(k, "Selected column: %lu with %lu elements\n", col->column.id, col->column.count);

	cover_column(col);
	step_print(k, "Covered column: %lu\n", col->column.id);
	
	for (struct dlx_node *row = col->down; row != col; row = row->down) {
		step_print(k, "Selected row: %lu\n", row->data.row_id);

		if (!matrix->solved) {
			// push row to solution set
			*solution_len += 1;
			*solution = realloc(*solution, *solution_len * sizeof(struct dlx_node*));
			(*solution)[*solution_len - 1] = row;
		}

		for (struct dlx_node *node = row->right; node != row; node = node->right) {
			step_print(k, "Covering adjacent column: %lu\n", node->data.parent->column.id);
			cover_column(node->data.parent);
		}

		if (dlx_solve_impl(matrix, k + 1, solution, solution_len)) {
			matrix->solved = true;
		}

		if (!matrix->solved) {
			// pop row from solution set
			*solution_len -= 1;
			(*solution)[*solution_len] = NULL;
			*solution = realloc(*solution, *solution_len * sizeof(struct dlx_node*));
		}

		for (struct dlx_node *node = row->left; node != row; node = node->left) {
			step_print(k, "Uncovering adjacent column: %lu\n", node->data.parent->column.id);
			uncover_column(node->data.parent);
		}
	}

	uncover_column(col);
	step_print(k, "Uncovered column: %lu\n", col->column.id);

	return matrix->solved;
}

bool dlx_solve(struct dlx_matrix *matrix, struct dlx_node ***solution, size_t *solution_len) {
	return dlx_solve_impl(matrix, 0, solution, solution_len);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Please pass the name of the input file as a parameter!\n");
		return -1;
	}

	FILE *input = fopen(argv[1], "r");

	if (!input) {
		printf("Could not open input file!\n");
		return -1;
	}

	if (!try_read_board(input)) {
		printf("Failed to parse board!\n");
		return -1;
	}

	fclose(input);

	board_print();
	
	struct dlx_matrix *board_repr = matrix_create();

	struct dlx_node **solution = NULL;
	size_t solution_len = 0;

	if (!dlx_solve(board_repr, &solution, &solution_len)) {
		printf("Failed to find a solution!\n");
		matrix_free(board_repr);
		free(solution);
		return -1;
	}

	matrix_free(board_repr);

	printf("Solution Found:\n");
	for (size_t i = 0; i < solution_len; i++) {
		unsigned int row_id = solution[i]->data.row_id;

		size_t row_idx = row_id / board_grid_size;
		size_t col_idx = (row_id / board_line_size) % board_line_size;
		size_t offset = (row_id % board_line_size) + 1;

		board_set(col_idx, row_idx, offset);
	}

	board_print();

	free(solution);

	return 0;
}
