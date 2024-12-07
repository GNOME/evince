# Contributing

## Licensing

Your work is considered a derivative work of the Evince codebase, and
therefore must be licensed as GPLv2+.

You do not need to assign us copyright attribution.
It is our belief that you should always retain copyright on your own work.

## Troubleshooting

To enable the debug messages, set the environment variable for the section
you want to debug or set `EV_DEBUG` to enable debug messages for all sections.

The following sections are available:

```c
EV_DEBUG_JOBS
EV_DEBUG_SHOW_BORDERS
```

#### Example
```c
EV_DEBUG_JOBS=1 evince document.pdf
```

### ‘Show borders’ debugging hint

Evince can show a border around the following graphical elements:

 * text characters
 * links
 * form elements
 * annotations
 * images
 * media elements
 * selections

this can be very helpful when debugging display issues related to those
elements, to activate it you just need to set two env vars when calling
Evince from a terminal, e.g. to show annotation borders:

```sh
EV_DEBUG=borders EV_DEBUG_SHOW_BORDERS=annots evince
```

where `EV_DEBUG_SHOW_BORDERS` can be set to any of the following values:
`chars` `links` `forms` `annots` `images` `media` `selections`.

If you need to add additional tracing macros to debug a problem, it is
probably a good idea to submit a patch to add them. Chances are someone
else will need to debug stuff in the future.

### Debug Poppler messages

Poppler is the library used by Evince to render PDF documents. When a document
presents error, or there are issues in Poppler to handle it, the output can be
seen by setting `G_MESSAGES_DEBUG` to enable debug messages for Poppler.

#### Example

```
G_MESSAGES_DEBUG=Poppler evince document.pdf
```

or

```
G_MESSAGES_DEBUG=all evince document.pdf
```


## Code Style

Evince follows the Linux coding style (K&R), with some mix of GObject
and Gtk coding style. However, the code has not evolved organically, and
there is a mix and match of coding style with respect to indentation.

```c
static gboolean
this_is_a_function (GtkWidget    *param,
                    const gchar  *another_param,
                    guint         third_param,
                    GError      **error)
{
	g_return_val_if_fail (GTK_IS_WIDGET (param), FALSE);
	g_return_val_if_fail (third_param > 10, FALSE);

	/*
	 * This is the preferred style for multi-line
	 * comments. Please use it consistently, and do not use
	 * C++ comment style.
	 */
	if (!do_some_more_work ()) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "Something failed");
		return FALSE;
	}

	if (another_param != NULL)
		return FALSE;
	else
		return TRUE;
}
```

```c
void      do_something_one   (GtkWidget  *widget);
void      do_something_two   (GtkWidget  *widget,
                              GError    **error);
gchar    *do_something_three (GtkWidget  *widget);
gboolean  do_something_four  (GtkWidget  *widget);
```

 * Notice that we use 8-space indention.
 * We do not use braces for one line blocks. For main blocks, the
   opening brace is its own line, for other statements, right after
   the statement.
 * Spaces before braces and parenthesis.
 * Always compare against `NULL` rather than implicit comparisons.
   This eases ports to other languages and readability.
 * Use #define for constants. Try to avoid “magic constants” or
   “magic numbers”.
 * Align function parameters.
 * Align blocks of function declarations in the header. This
   vastly improves readability and scanning to find what you want.

If in doubt, look for examples elsewhere in the codebase, especially
in the same file you are editing.  Also, the Linux kernel coding style
in https://www.kernel.org/doc/html/latest/process/coding-style.html

### Commit messages

The expected format for git commit messages is as follows:

```plain
Short explanation of the commit

Longer explanation explaining exactly what's changed, whether any
external or private interfaces changed, what bugs were fixed (with bug
tracker reference if applicable) and so forth. Be concise but not too
brief.

Closes #1234
```

 - Always add a brief description of the commit to the _first_ line of
 the commit and terminate by two newlines (it will work without the
 second newline, but that is not nice for the interfaces).

 - Whenever possible, the first line should include the subsystem of
   the evince the commit belongs: `shell`, `libdocument`, `libview`,
   `libmisc`, `backends`, `cut-n-paste`, `build`, `doc`, `flatpak`.
   e.g. “flatpak: bump version of poppler”

 - First line (the brief description) must only be one sentence and
 should start with a capital letter unless it starts with a lowercase
 symbol or identifier. Don't use a trailing period either. Aim to not
 exceed 72 characters.

 - The main description (the body) is normal prose and should use normal
 punctuation and capital letters where appropriate. Consider the commit
 message as an email sent to the developers (or yourself, six months
 down the line) detailing **why** you changed something. There's no need
 to specify the **how**: the changes can be inlined.

 - While adding the main description please make sure that individual lines
within the body are no longer than 80 columns, ideally a bit less. This makes
it easier to read without scrolling (both in GitLab as well as a terminal with
the default terminal size).

 - When committing code on behalf of others use the `--author` option, e.g.
 `git commit -a --author "Joe Coder <joe@coder.org>"` and `--signoff`.

 - If your commit is addressing an issue, use the
 [GitLab syntax](https://docs.gitlab.com/ce/user/project/issues/managing_issues.html#default-closing-pattern)
 to automatically close the issue when merging the commit with the upstream
 repository:

```plain
Closes #1234
Fixes #1234
Closes: https://gitlab.gnome.org/GNOME/gtk/issues/1234
```

 - If you have a merge request with multiple commits and none of them
 completely fixes an issue, you should add a reference to the issue in
 the commit message, e.g. `Bug: #1234`, and use the automatic issue
 closing syntax in the description of the merge request.
