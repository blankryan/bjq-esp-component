# Troubleshooting & Error Handling — `__COMPONENT_NAME__`

## Error Handling

| Return Code | Cause | Recommended Recovery |
|---|---|---|
| `ESP_OK` | Operation succeeded | None |
| `ESP_ERR_INVALID_ARG` | `NULL` config or handle passed | Validate caller pointers before invoking API |
| `ESP_ERR_NO_MEM` | Heap allocation failed during init | Inspect free heap (`esp_get_free_heap_size()`) |

## Known Limitations

- Current implementation is a verified starter scaffold; peripheral/protocol operations are not implemented yet.
- Runtime hardware tests have not yet been executed in CI.
