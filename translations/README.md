# Translations

IKCLUT Studio uses Qt Linguist translations.

The build compiles `IkClutStudio_zh_CN.ts` into a `.qm` file and embeds it under
the `:/i18n` resource prefix. At startup the app tries the system locale first,
then falls back to `IkClutStudio_zh_CN`.

Typical maintenance flow:

```powershell
cmake --build build --target update_translations
cmake --build build --target release_translations
```

When adding user-visible text in C++ UI code, wrap it in `tr("...")` so Qt
Linguist can extract it.
