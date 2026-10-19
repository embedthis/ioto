#!/usr/bin/env bash
#
#   prep.sh - TestMe prep script
#
#   Generates the certificate matrix used by tls-verify.tst.c. Every certificate is signed by
#   the shared test CA in ../certs so the client can trust the issuer and still reject the
#   certificate on its own merits (expiry, validity window, hostname).
#
#   WARNING: test certificates only -- never use in production.
#
#   The certificate matrix is a fixture, not a test. A generation failure must not fail the whole
#   r group: tls-verify.tst.c skips each case whose certificate is missing, so an environment
#   without a working openssl reports 23 passing tests and a stated skip rather than a red prep.
#   So this script does not run under "set -e" -- it generates what it can, then checks the
#   result and exits 0 either way, saying which it was.
#

CERTS=.certs
CA=../certs/ca.crt
CAKEY=../certs/ca.key

if [ ! -f "$CA" -o ! -f "$CAKEY" ] ; then
    echo "   [Skip] No test CA at $CA -- skipping TLS certificate generation" >&2
    exit 0
fi

#
#   Every openssl call below discards its stderr and runs under "set -e", so an absent openssl
#   ends the prep script with exit 1 and no diagnosis, which TestMe reports as a failed prep for
#   the whole group. Decide it here instead, and skip: tls-verify.tst.c already skips each case
#   whose certificate is missing, so a machine without openssl reports honestly rather than red.
#
if ! command -v openssl >/dev/null 2>&1 ; then
    echo "   [Skip] No openssl command on PATH -- skipping TLS certificate generation" >&2
    exit 0
fi

#   The first invocation keeps its stderr, so a broken openssl says why rather than failing mute
if ! openssl version >/dev/null ; then
    echo "   [Skip] openssl is present but not runnable -- skipping TLS certificate generation" >&2
    exit 0
fi

#   Fixtures depend on the active CA, OpenSSL and the current validity window. Recreate them
#   rather than accepting cached certificates copied from another checkout or release archive.
echo "   [Create] TLS test certificates in $CERTS"
rm -rf $CERTS
mkdir -p $CERTS

#
#   "openssl ca" needs its own small database to revoke against, so one is built here rather
#   than in ../certs: nothing outside this directory should be written by a test prep script.
#
CADB=$CERTS/ca-db
mkdir -p $CADB
touch $CADB/index.txt
echo 1000 > $CADB/serial
echo 1000 > $CADB/crlnumber

cat > $CADB/ca.conf <<EOF
[ca]
default_ca = test_ca

[test_ca]
dir               = $CADB
database          = \$dir/index.txt
serial            = \$dir/serial
crlnumber         = \$dir/crlnumber
new_certs_dir     = \$dir
certificate       = $CA
private_key       = $CAKEY
default_md        = sha256
default_days      = 3650
default_crl_days  = 3650
policy            = test_policy
email_in_dn       = no
rand_serial       = no
unique_subject    = no
copy_extensions   = copy

[test_policy]
commonName        = supplied
EOF


#
#   $1 = base name, $2 = common name, $3 = -days, $4 = extra x509 options
#
make-cert() {
    NAME=$1
    CN=$2
    DAYS=$3
    shift 3

    openssl genrsa -out $CERTS/$NAME.key 2048 2>/dev/null
    openssl req -new -key $CERTS/$NAME.key -out $CERTS/$NAME.csr -subj "/CN=$CN" 2>/dev/null
    printf 'subjectAltName=DNS:%s\nbasicConstraints=CA:FALSE\n' "$CN" > $CERTS/$NAME.ext
    openssl x509 -req -in $CERTS/$NAME.csr -CA $CA -CAkey $CAKEY -CAserial $CERTS/ca.srl -CAcreateserial \
        -out $CERTS/$NAME.crt -days $DAYS -extfile $CERTS/$NAME.ext "$@" 2>/dev/null
    rm -f $CERTS/$NAME.csr $CERTS/$NAME.ext
}

#   Valid: correct hostname, currently valid, signed by the test CA
make-cert valid localhost 3650

#   Wrong hostname: valid in every respect except the name the client asks for
make-cert wronghost other.example.com 3650

# IP identity without DNS SANs: $1 is the base name; $2 is the IP address.
make-ipcert() {
    NAME=$1
    IP=$2

    openssl genrsa -out $CERTS/$NAME.key 2048 2>/dev/null
    openssl req -new -key $CERTS/$NAME.key -out $CERTS/$NAME.csr -subj "/CN=device" 2>/dev/null
    printf 'subjectAltName=IP:%s\nbasicConstraints=CA:FALSE\n' "$IP" > $CERTS/$NAME.ext
    openssl x509 -req -in $CERTS/$NAME.csr -CA $CA -CAkey $CAKEY -CAserial $CERTS/ca.srl -CAcreateserial \
        -out $CERTS/$NAME.crt -days 3650 -extfile $CERTS/$NAME.ext 2>/dev/null
    rm -f $CERTS/$NAME.csr $CERTS/$NAME.ext
}

#   Matches the loopback address the IP literal test connects to, and one that does not
make-ipcert ipsan 127.0.0.1
make-ipcert ipwrong 10.9.9.9

# Use leaf + intermediate to verify that a root-trusting client receives the full chain.
openssl genrsa -out $CERTS/intermediate.key 2048 2>/dev/null
openssl req -new -key $CERTS/intermediate.key -out $CERTS/intermediate.csr -subj "/CN=Test Intermediate CA" 2>/dev/null
printf 'basicConstraints=critical,CA:TRUE,pathlen:0\nkeyUsage=critical,keyCertSign,cRLSign\n' > $CERTS/intermediate.ext
openssl x509 -req -in $CERTS/intermediate.csr -CA $CA -CAkey $CAKEY -CAserial $CERTS/ca.srl -CAcreateserial \
    -out $CERTS/intermediate.crt -days 3650 -extfile $CERTS/intermediate.ext 2>/dev/null
rm -f $CERTS/intermediate.csr $CERTS/intermediate.ext

openssl genrsa -out $CERTS/chain.key 2048 2>/dev/null
openssl req -new -key $CERTS/chain.key -out $CERTS/chain.csr -subj "/CN=localhost" 2>/dev/null
printf 'subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\n' > $CERTS/chain.ext
openssl x509 -req -in $CERTS/chain.csr -CA $CERTS/intermediate.crt -CAkey $CERTS/intermediate.key -CAcreateserial \
    -out $CERTS/chain-leaf.crt -days 3650 -extfile $CERTS/chain.ext 2>/dev/null
cat $CERTS/chain-leaf.crt $CERTS/intermediate.crt > $CERTS/chain.crt
rm -f $CERTS/chain.csr $CERTS/chain.ext $CERTS/chain-leaf.crt

# Exceed ME_R_TLS_MAX_DEPTH without certificate path-length constraints.
DEEPCHAIN_DEPTH=9
DEEPSERIAL=$CERTS/deepchain.srl
prevCert=$CA
prevKey=$CAKEY
prevSerial="$CERTS/ca.srl"
for i in $(seq 1 $DEEPCHAIN_DEPTH) ; do
    NAME=$CERTS/deepchain-int$i
    openssl genrsa -out $NAME.key 2048 2>/dev/null
    openssl req -new -key $NAME.key -out $NAME.csr -subj "/CN=Test Deep Intermediate $i" 2>/dev/null
    printf 'basicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign,cRLSign\n' > $NAME.ext
    openssl x509 -req -in $NAME.csr -CA $prevCert -CAkey $prevKey -CAserial $prevSerial -CAcreateserial \
        -out $NAME.crt -days 3650 -extfile $NAME.ext 2>/dev/null
    rm -f $NAME.csr $NAME.ext
    prevCert=$NAME.crt
    prevKey=$NAME.key
    prevSerial=$DEEPSERIAL
done

openssl genrsa -out $CERTS/deepchain.key 2048 2>/dev/null
openssl req -new -key $CERTS/deepchain.key -out $CERTS/deepchain.csr -subj "/CN=localhost" 2>/dev/null
printf 'subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\n' > $CERTS/deepchain.ext
openssl x509 -req -in $CERTS/deepchain.csr -CA $prevCert -CAkey $prevKey -CAserial $DEEPSERIAL -CAcreateserial \
    -out $CERTS/deepchain-leaf.crt -days 3650 -extfile $CERTS/deepchain.ext 2>/dev/null
rm -f $CERTS/deepchain.csr $CERTS/deepchain.ext

cat $CERTS/deepchain-leaf.crt > $CERTS/deepchain.crt
for i in $(seq $DEEPCHAIN_DEPTH -1 1) ; do
    cat $CERTS/deepchain-int$i.crt >> $CERTS/deepchain.crt
done
rm -f $CERTS/deepchain-leaf.crt
for i in $(seq 1 $DEEPCHAIN_DEPTH) ; do
    rm -f $CERTS/deepchain-int$i.key $CERTS/deepchain-int$i.crt
done

#   Expired: notAfter in the past
openssl genrsa -out $CERTS/expired.key 2048 2>/dev/null
openssl req -new -key $CERTS/expired.key -out $CERTS/expired.csr -subj "/CN=localhost" 2>/dev/null
printf 'subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\n' > $CERTS/expired.ext
openssl ca -batch -config $CADB/ca.conf -in $CERTS/expired.csr \
    -out $CERTS/expired.crt -extfile $CERTS/expired.ext \
    -startdate 20200101000000Z -enddate 20200102000000Z 2>/dev/null
rm -f $CERTS/expired.csr $CERTS/expired.ext

#   Not yet valid: notBefore in the future
openssl genrsa -out $CERTS/notyet.key 2048 2>/dev/null
openssl req -new -key $CERTS/notyet.key -out $CERTS/notyet.csr -subj "/CN=localhost" 2>/dev/null
printf 'subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\n' > $CERTS/notyet.ext
openssl ca -batch -config $CADB/ca.conf -in $CERTS/notyet.csr \
    -out $CERTS/notyet.crt -extfile $CERTS/notyet.ext \
    -startdate 20900101000000Z -enddate 20900102000000Z 2>/dev/null
rm -f $CERTS/notyet.csr $CERTS/notyet.ext

#   Self-signed: no path to the test CA at all
openssl req -new -x509 -newkey rsa:2048 -nodes -keyout $CERTS/selfsigned.key \
    -out $CERTS/selfsigned.crt -days 3650 -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost" 2>/dev/null

#
#   Revoked: valid in every respect - correct hostname, in date, signed by the test CA - and
#   named on the revocation list. It is the only case that a client can accept or reject purely
#   on whether it consults the CRL, which is what makes it the pin for the CRL enforcement fix.

make-revocable() {
    NAME=$1
    openssl genrsa -out $CERTS/$NAME.key 2048 2>/dev/null
    openssl req -new -key $CERTS/$NAME.key -out $CERTS/$NAME.csr -subj "/CN=localhost" \
        -addext "subjectAltName=DNS:localhost" -addext "basicConstraints=CA:FALSE" 2>/dev/null
    openssl ca -batch -config $CADB/ca.conf -in $CERTS/$NAME.csr -out $CERTS/$NAME.crt 2>/dev/null
    rm -f $CERTS/$NAME.csr
}

#   Two certificates from the same CA database: one is revoked, the other is not. The second is
#   what proves a client with a CRL configured still completes a handshake, so a test that sees
#   the revoked one rejected is measuring revocation and not simply a broken CRL configuration.
make-revocable revoked
make-revocable notrevoked

#   An empty list, generated before anything is revoked
openssl ca -batch -config $CADB/ca.conf -gencrl -out $CERTS/empty.crl 2>/dev/null

openssl ca -batch -config $CADB/ca.conf -revoke $CERTS/revoked.crt 2>/dev/null
openssl ca -batch -config $CADB/ca.conf -gencrl -out $CERTS/revoked.crl 2>/dev/null

rm -f $CERTS/*.srl

#
#   What tls-verify.tst.c needs. Report rather than fail: the exit status is 0 either way, and the
#   test skips the cases whose fixtures are absent.
#
missing=
for f in valid wronghost ipsan ipwrong chain deepchain expired notyet selfsigned revoked notrevoked ; do
    [ -f "$CERTS/$f.crt" ] || missing="$missing $f.crt"
done
for f in empty.crl revoked.crl ; do
    [ -f "$CERTS/$f" ] || missing="$missing $f"
done
if [ -n "$missing" ] ; then
    echo "   [Skip] openssl did not produce:$missing -- the affected TLS cases will skip" >&2
else
    echo "   [Done] TLS test certificates in $CERTS"
fi
exit 0
