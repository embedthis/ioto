# test/cloud - Cloud Integration Tests

Integration tests for Ioto cloud connectivity including AWS IoT Core, MQTT messaging, device synchronization, and cloud metrics.

## Test Coverage

### AWS Integration ([aws/](aws/))
- `cloudwatch.tst.sh` - CloudWatch metrics publishing and validation

### Metrics ([metric/](metric/))
- `getset.tst.sh` - Cloud metrics get/set operations

### MQTT Protocol ([mqtt/](mqtt/))
- `ping.tst.sh` - MQTT connection and keep-alive testing
- `request.tst.sh` - MQTT request/response patterns
- `store.tst.sh` - MQTT message persistence and storage

### Device Shadow ([shadow/](shadow/))
- Device shadow synchronization tests

### State Management ([state/](state/))
- Cloud state synchronization and persistence

### Cloud Sync ([sync/](sync/))
- `store.tst.sh` - Device-to-cloud data synchronization

### HTTP Client ([url/](url/))
- `get.tst.sh` - HTTP/HTTPS client operations

## Running Tests

Run all cloud tests:
```bash
cd /Users/mob/c/agent/test
tm cloud
```

Run specific test category:
```bash
tm cloud/mqtt               # MQTT protocol tests
tm cloud/aws                # AWS integration tests
tm cloud/sync               # Synchronization tests
```

Run individual test:
```bash
tm cloud/mqtt/ping.tst.sh   # Specific MQTT test
```

## Test Environment

### Local Testing
- Tests can run against local MQTT broker for basic connectivity
- Uses test certificates from `../../certs/` for TLS
- Creates isolated state directories per test

### Cloud Testing
For full cloud integration tests, configure:
- AWS IoT Core endpoint in test configuration
- Valid device certificates in state directory
- Appropriate IAM permissions for CloudWatch

## Prerequisites

- Ioto agent built with: `make APP=unit`
- TestMe installed: `tm --version`
- OpenSSL or MbedTLS libraries
- Network connectivity for cloud tests
- Optional: AWS IoT Core account for full integration tests

## Security Notes

- Never commit real AWS certificates or credentials
- Use test certificates for local/development testing
- Cloud tests may be skipped if credentials not available
- See `../../CLAUDE.md` for security guidelines

## See Also

- [Parent test documentation](../README.md)
- [Ioto MQTT Client](../../paks/mqtt/README.md)
- [Ioto URL Client](../../paks/url/README.md)
- [Ioto Database Sync](../../paks/db/README.md)
- [AWS IoT Core Documentation](https://docs.aws.amazon.com/iot/)
