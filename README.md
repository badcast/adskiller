# adskiller

The API endpoint can be set at configure time with a root `.env` file:

```dotenv
NREMOTEADDR=https://your-api.example/api
```

Alternatively, set `NREMOTEADDR` in the environment before configuring CMake.
The legacy format containing only the endpoint URL on the first non-comment
line is also supported. `.env.example` shows the expected variable format.