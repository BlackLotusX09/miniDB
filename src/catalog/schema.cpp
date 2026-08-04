#include "catalog/schema.h"

// Returns the 0-based index of the column with the given name, or -1 if not found.
int Schema::GetColumnIndex(const std::string& name) const {
    for (int i = 0; i < static_cast<int>(columns_.size()); i++) {
        if (columns_[i].name == name) {
            return i;
        }
    }
    return -1;
}
