# Procedures

This document provides an overview of development procedures for the Ioto Device Agent.

## Index

- [Testing Procedures](#testing-procedures)
- [Build Procedures](#build-procedures)
- [Release Procedures](#release-procedures)
- [Documentation Procedures](#documentation-procedures)

## Testing Procedures

### Unit Testing

Run the complete test suite:

```bash
make APP=unit && make run
```

Run specific tests:

```bash
cd test && testme NAME
```

### Integration Testing

Integration tests verify component interactions. Run from the test directory:

```bash
cd test && testme mqtt-*      # MQTT tests
cd test && testme db-sync-*   # Database sync tests
```

## Build Procedures

### Development Build

```bash
make APP=demo
```

### Production Build

```bash
DEBUG=release ME_R_LOGGING=0 make
```

### Cross-Platform Build

See `README-CROSS.md` for cross-compilation instructions.

## Release Procedures

1. Update version numbers
2. Update CHANGELOG.md
3. Run full test suite
4. Build release packages
5. Create release notes in doc/releases/
6. Tag the release in git

## Documentation Procedures

### Generate API Documentation

```bash
make doc
```

### Update Design Documents

When making architectural changes:

1. Update AI/designs/DESIGN.md
2. Create component-specific design documents as needed
3. Update CLAUDE.md if conventions change
