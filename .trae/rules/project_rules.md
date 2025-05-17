# Project Rules

These rules are designed to ensure that the AI agent's actions and tool uses align with the project's objectives and workflow.

---

### Tool Use and Workflow

1. **Use tools step-by-step:** Only one tool per message, and each tool use should be informed by the result of the previous tool use.
2. **Wait for user confirmation:** After each tool use, wait for the user's response before proceeding.
3. **Tool use formatting:** Use the specified XML-style tags for tool use (e.g., `<read_file>`, `<replace_in_file>`, etc.).
4. **Choose the right tool:** Use `replace_in_file` for targeted edits, `write_to_file` for creating or overwriting entire files, and other tools as appropriate for the task.
5. **Do not ask unnecessary questions:** Use available tools to gather information instead of asking the user, unless a required parameter is missing.
6. **Do not assume tool success:** Always wait for explicit confirmation from the user that a tool use succeeded before proceeding.
7. **No back-and-forth:** Do not end responses with questions or offers for further assistance; be direct and final in completion messages.
8. **No conversational language:** Do not start messages with "Great", "Certainly", "Okay", "Sure", etc. Be clear and technical.
9. **No partial line matches in replace_in_file:** SEARCH blocks must include complete lines, not fragments.
10. **Order SEARCH/REPLACE blocks:** If using multiple blocks, list them in the order they appear in the file.
11. **Use the final state of files:** After editing, use the updated file content as the reference for further changes, accounting for any auto-formatting.
12. **No directory changes:** All operations must be performed from the current working directory; do not attempt to `cd` elsewhere.
13. **Commands must be tailored:** Before executing commands, consider the user's OS and shell, and prepend with `cd` if the command must run in a different directory.
14. **Use list_files and search_files for discovery:** Prefer these tools over running shell commands like `ls` for file and directory exploration.
15. **Use list_code_definition_names for code structure:** Use this tool to get an overview of source code definitions in a directory.
16. **Use execute_command for CLI operations:** Provide a clear explanation for each command, and set `requires_approval` appropriately.
17. **Use MCP tools/resources as available:** Prefer MCP tools/resources over browser actions if both are available for a task.
18. **Browser actions:** Only use browser actions when necessary, and always start with `launch` and end with `close`.
19. **Auto-formatting awareness:** Be aware that file edits may be auto-formatted; always use the resulting file state for subsequent edits.
20. **No use of ~ or $HOME:** Always use explicit paths, never home directory shortcuts.

---

### Objective and Task Management

1. **Break tasks into clear steps:** Analyze the user's task, set clear goals, and work through them sequentially.
2. **Summarize and document:** When creating a new task, provide a detailed summary including current work, key concepts, relevant files, problem-solving, and pending tasks.
3. **No unnecessary conversation:** Focus on accomplishing the user's task, not on engaging in conversation.

---

### Special Operations

1. **To move code:** Use two SEARCH/REPLACE blocks (one to delete from the original, one to insert at the new location).
2. **To delete code:** Use an empty REPLACE section.

---

### System and Environment

1. **Current working directory:** All operations are performed from the specified working directory.
2. **Consider actively running terminals:** Check for active processes before running commands that might conflict.

---

These rules will be followed in all subsequent actions and tool uses.
