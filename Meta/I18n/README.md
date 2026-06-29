### Localization structure

- each language gets its own JSON file
- the basis for each language is always en_US.json

The structure of the JSON file is the following:

```json
{
  "static": {
    "foo": "bar",
    "baz": {
      "something": "Something"
    }
  },
  "generic": {
    "ok": "Ok"
  },
  "menu": {
    "file": "File"
  },
  "some_window": {
    "title": "Some Window",
    "some_label": "Some Label"
  }
}
```

I.e., first come static labels. Static labels are labels that appear anywhere in the UI that do not require appending
`##<key>` or `###<key>` to translated string, which is a requirement of imgui in most but not all cases. So for example,
to get the text for an OK button you'd use `TR("generic.ok")` which would appear in the application as `Ok` but the
actual
translation value passed to imgui would be `Ok##generic.ok`, and imgui would just remove the appended `##generic.ok`.
In the example above the translation "Something" would be referenced in code via `TR("baz.something")`, i.e., the
`static` part is left
out as it is only used to tell the localization system to not append `##baz.something`. Nested keys like `baz.something`
should be used where it makes sense. I.e.

```json
{
  "static": {
    "state": {
      "unknown": "Unknown",
      "active": "Active"
    }
  }
}
```

For windows the structure is always

```json
{
  "some_window": {
    "title": "Window title",
    "other_keys": ""
  }
}
```

To get the translated window title you'd then use `TR("some_window.title")`. Titles have the key appended as
`Window Title###some_window.title`,
which is once again a requirement of imgui for identifying windows.

Lastly, for the settings window the structure is

```json
{
  "settings": {
    "title": "Settings",
    "some_settings_group": {
      "header": "Group name"
    }
  }
}
```