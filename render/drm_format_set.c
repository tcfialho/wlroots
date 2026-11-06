#include <assert.h>
#include <drm_fourcc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/util/log.h>
#include "render/drm_format_set.h"

void wlr_drm_format_finish(struct wlr_drm_format *format) {
	if (!format) {
		return;
	}

	free(format->modifiers);
	*format = (struct wlr_drm_format){0};
}

void wlr_drm_format_set_finish(struct wlr_drm_format_set *set) {
	for (size_t i = 0; i < set->len; ++i) {
		wlr_drm_format_finish(&set->formats[i]);
	}
	free(set->formats);

	set->len = 0;
	set->capacity = 0;
	set->formats = NULL;
}

static struct wlr_drm_format *format_set_get(const struct wlr_drm_format_set *set,
		uint32_t format) {
	for (size_t i = 0; i < set->len; ++i) {
		if (set->formats[i].format == format) {
			return &set->formats[i];
		}
	}

	return NULL;
}

const struct wlr_drm_format *wlr_drm_format_set_get(
		const struct wlr_drm_format_set *set, uint32_t format) {
	return format_set_get(set, format);
}

bool wlr_drm_format_set_has(const struct wlr_drm_format_set *set,
		uint32_t format, uint64_t modifier) {
	const struct wlr_drm_format *fmt = wlr_drm_format_set_get(set, format);
	if (!fmt) {
		return false;
	}
	return wlr_drm_format_has(fmt, modifier);
}

bool wlr_drm_format_set_add(struct wlr_drm_format_set *set, uint32_t format,
		uint64_t modifier) {
	assert(format != DRM_FORMAT_INVALID);

	struct wlr_drm_format *existing = format_set_get(set, format);
	if (existing) {
		return wlr_drm_format_add(existing, modifier);
	}

	struct wlr_drm_format fmt;
	wlr_drm_format_init(&fmt, format);
	if (!wlr_drm_format_add(&fmt, modifier)) {
		wlr_drm_format_finish(&fmt);
		return false;
	}

	if (set->len == set->capacity) {
		size_t capacity = set->capacity ? set->capacity * 2 : 4;

		struct wlr_drm_format *fmts = realloc(set->formats, sizeof(*fmts) * capacity);
		if (!fmts) {
			wlr_log_errno(WLR_ERROR, "Allocation failed");
			wlr_drm_format_finish(&fmt);
			return false;
		}

		set->capacity = capacity;
		set->formats = fmts;
	}

	set->formats[set->len++] = fmt;
	return true;
}

bool wlr_drm_format_set_remove(struct wlr_drm_format_set *set, uint32_t format,
		uint64_t modifier) {
	struct wlr_drm_format *fmt = format_set_get(set, format);
	if (fmt == NULL) {
		return false;
	}

	for (size_t idx = 0; idx < fmt->len; idx++) {
		if (fmt->modifiers[idx] == modifier) {
			memmove(&fmt->modifiers[idx], &fmt->modifiers[idx+1], (fmt->len - idx - 1) * sizeof(fmt->modifiers[0]));
			fmt->len--;
			return true;
		}
	}
	return false;
}

void wlr_drm_format_init(struct wlr_drm_format *fmt, uint32_t format) {
	*fmt = (struct wlr_drm_format){
		.format = format,
	};
}

bool wlr_drm_format_has(const struct wlr_drm_format *fmt, uint64_t modifier) {
	for (size_t i = 0; i < fmt->len; ++i) {
		if (fmt->modifiers[i] == modifier) {
			return true;
		}
	}
	return false;
}

bool wlr_drm_format_add(struct wlr_drm_format *fmt, uint64_t modifier) {
	if (wlr_drm_format_has(fmt, modifier)) {
		return true;
	}

	if (fmt->len == fmt->capacity) {
		size_t capacity = fmt->capacity ? fmt->capacity * 2 : 4;

		uint64_t *new_modifiers = realloc(fmt->modifiers, sizeof(*fmt->modifiers) * capacity);
		if (!new_modifiers) {
			wlr_log_errno(WLR_ERROR, "Allocation failed");
			return false;
		}

		fmt->capacity = capacity;
		fmt->modifiers = new_modifiers;
	}

	fmt->modifiers[fmt->len++] = modifier;
	return true;
}

bool wlr_drm_format_copy(struct wlr_drm_format *dst, const struct wlr_drm_format *src) {
	assert(src->len <= src->capacity);

	if (dst == src) {
		return true;
	}

	size_t len = src->len;
	uint32_t drm_format = src->format;

	if (dst->capacity < len) {
		uint64_t *modifiers = realloc(dst->modifiers, sizeof(*modifiers) * len);
		if (!modifiers) {
			return false;
		}
		dst->modifiers = modifiers;
		dst->capacity = len;
	}

	if (len > 0) {
		memcpy(dst->modifiers, src->modifiers, sizeof(*dst->modifiers) * len);
	}
	dst->len = len;
	dst->format = drm_format;
	return true;
}

bool wlr_drm_format_set_copy(struct wlr_drm_format_set *dst, const struct wlr_drm_format_set *src) {
	if (dst == src) {
		return true;
	}

	struct wlr_drm_format *formats = malloc(src->len * sizeof(formats[0]));
	if (formats == NULL && src->len > 0) {
		return false;
	}

	struct wlr_drm_format_set out = {
		.len = 0,
		.capacity = src->len,
		.formats = formats,
	};

	for (size_t i = 0; i < src->len; i++) {
		out.formats[out.len] = (struct wlr_drm_format){0};
		if (!wlr_drm_format_copy(&out.formats[out.len], &src->formats[i])) {
			wlr_drm_format_set_finish(&out);
			return false;
		}
		out.len++;
	}

	wlr_drm_format_set_finish(dst);
	*dst = out;
	return true;
}


bool wlr_drm_format_intersect(struct wlr_drm_format *dst,
		const struct wlr_drm_format *a, const struct wlr_drm_format *b) {
	assert(a->format == b->format);

	if (a == b) {
		return wlr_drm_format_copy(dst, a);
	}

	size_t max_capacity = a->len < b->len ? a->len : b->len;
	if (max_capacity == 0) {
		wlr_drm_format_finish(dst);
		wlr_drm_format_init(dst, a->format);
		return true;
	}

	uint64_t stack_modifiers[64];
	uint64_t *modifiers = stack_modifiers;
	if (max_capacity > 64) {
		modifiers = malloc(sizeof(*modifiers) * max_capacity);
		if (!modifiers) {
			return false;
		}
	}

	size_t len = 0;
	for (size_t i = 0; i < a->len; i++) {
		for (size_t j = 0; j < b->len; j++) {
			if (a->modifiers[i] == b->modifiers[j]) {
				modifiers[len++] = a->modifiers[i];
				break;
			}
		}
	}

	if (len == 0) {
		if (modifiers != stack_modifiers) {
			free(modifiers);
		}
		wlr_drm_format_finish(dst);
		wlr_drm_format_init(dst, a->format);
		return true;
	}

	uint64_t *final_modifiers = NULL;
	if (modifiers == stack_modifiers) {
		if (dst->capacity >= len) {
			memmove(dst->modifiers, stack_modifiers, sizeof(*dst->modifiers) * len);
			dst->len = len;
			dst->format = a->format;
			return true;
		} else {
			final_modifiers = malloc(sizeof(*final_modifiers) * len);
			if (!final_modifiers) {
				return false;
			}
			memcpy(final_modifiers, stack_modifiers, sizeof(*final_modifiers) * len);
		}
	} else {
		final_modifiers = realloc(modifiers, sizeof(*final_modifiers) * len);
		if (!final_modifiers) {
			final_modifiers = modifiers;
		}
	}

	wlr_drm_format_finish(dst);
	dst->capacity = len;
	dst->len = len;
	dst->modifiers = final_modifiers;
	dst->format = a->format;
	return true;
}

bool wlr_drm_format_set_intersect(struct wlr_drm_format_set *dst,
		const struct wlr_drm_format_set *a, const struct wlr_drm_format_set *b) {
	if (a->len == 0 || b->len == 0) {
		return false;
	}

	size_t max_capacity = a->len < b->len ? a->len : b->len;
	struct wlr_drm_format stack_formats[32];
	struct wlr_drm_format *formats = stack_formats;
	if (max_capacity > 32) {
		formats = malloc(sizeof(*formats) * max_capacity);
		if (formats == NULL) {
			wlr_log_errno(WLR_ERROR, "Allocation failed");
			return false;
		}
	}

	size_t len = 0;
	for (size_t i = 0; i < a->len; i++) {
		for (size_t j = 0; j < b->len; j++) {
			if (a->formats[i].format == b->formats[j].format) {
				struct wlr_drm_format fmt = {0};
				if (!wlr_drm_format_intersect(&fmt, &a->formats[i], &b->formats[j])) {
					for (size_t k = 0; k < len; k++) {
						wlr_drm_format_finish(&formats[k]);
					}
					if (formats != stack_formats) {
						free(formats);
					}
					return false;
				}

				if (fmt.len > 0) {
					formats[len++] = fmt;
				} else {
					wlr_drm_format_finish(&fmt);
				}
				break;
			}
		}
	}

	if (len == 0) {
		if (formats != stack_formats) {
			free(formats);
		}
		return false;
	}

	struct wlr_drm_format *final_formats;
	if (formats == stack_formats) {
		final_formats = malloc(sizeof(*final_formats) * len);
		if (final_formats == NULL) {
			for (size_t k = 0; k < len; k++) {
				wlr_drm_format_finish(&formats[k]);
			}
			return false;
		}
		memcpy(final_formats, stack_formats, sizeof(*final_formats) * len);
	} else {
		final_formats = realloc(formats, sizeof(*final_formats) * len);
		if (final_formats == NULL) {
			final_formats = formats;
		}
	}

	wlr_drm_format_set_finish(dst);
	dst->capacity = len;
	dst->len = len;
	dst->formats = final_formats;
	return true;
}

static bool drm_format_set_extend(struct wlr_drm_format_set *dst,
		const struct wlr_drm_format_set *src) {
	for (size_t i = 0; i < src->len; i++) {
		struct wlr_drm_format *format = &src->formats[i];
		for (size_t j = 0; j < format->len; j++) {
			if (!wlr_drm_format_set_add(dst, format->format, format->modifiers[j])) {
				wlr_log_errno(WLR_ERROR, "Adding format/modifier to set failed");
				return false;
			}
		}
	}

	return true;
}

bool wlr_drm_format_set_union(struct wlr_drm_format_set *dst,
		const struct wlr_drm_format_set *a, const struct wlr_drm_format_set *b) {
	if (a->len == 0 && b->len == 0) {
		wlr_drm_format_set_finish(dst);
		return true;
	} else if (a->len == 0) {
		return wlr_drm_format_set_copy(dst, b);
	} else if (b->len == 0) {
		return wlr_drm_format_set_copy(dst, a);
	}

	struct wlr_drm_format_set out = {0};
	out.capacity = a->len + b->len;
	out.formats = malloc(sizeof(*out.formats) * out.capacity);
	if (out.formats == NULL) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		return false;
	}

	if (!drm_format_set_extend(&out, a) ||
		!drm_format_set_extend(&out, b)) {
		wlr_drm_format_set_finish(&out);
		return false;
	}

	wlr_drm_format_set_finish(dst);
	*dst = out;

	return true;
}
