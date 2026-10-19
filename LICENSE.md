# EmbedThis Ioto License

This software is distributed under three licenses:

* [GPL License](http://www.gnu.org/licenses/gpl-2.0.html)
* [EmbedThis Commercial License](https://www.embedthis.com/about/terms/)
* [EmbedThis Evaluation License](./EVAL.md)

The GPL License does not generally permit incorporating this software into non-open source programs. Commercial licenses for this software and support services are available from Embedthis Pty Ltd.

The commercial license enables you to embed the software and distribute and sell your products containing the software. With a paid-up subscription you can incorporate new releases, updates and patches for the software into your products. If you do not have an active subscription, you cannot apply commerical patches from the software to your products.

The eval license is for shorter-term commercial evaluations.

The EmbedThis Ioto service is provided subject to a commercial license and subscription as described in the [Terms of Use](https://www.embedthis.com/about/terms.html).

Please read the following document for more information or contact [sales@embedthis.com](mailto:sales@embedthis.com) if you have questions.

- [EmbedThis License Plans](https://www.embedthis.com/doc/builder/plans/overview/)

## EU Cyber Resilience Act

The GPL release of this software is provided without CRA conformity documentation. A manufacturer that incorporates it in a product placed on the EU market is responsible for that product's conformity with the Cyber Resilience Act. CRA conformity, including the Declaration of Conformity, user information, software bill of materials and vulnerability handling, is offered with the commercial license.

## Third Party Licenses

The Software contains third party software under the agreements that follow.

### RedBlack License

Portions used in rb.c

- Portion copyright (c) 2019 xieqing. https://github.com/xieqing
- License on GitHub: "May be freely redistributed, but copyright notice must be retained"

## MQTT portions

MIT License

Copyright (c) 2018 Liam Bindle

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## UTF-8 Decoder

Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
    See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

## libucontext (ISC License)

The fiber context-switch code in `lib/uctxLib.c` and `lib/uctxAssembly.S` is derived from libucontext, https://github.com/kaniini/libucontext.

Portions copyright (c) 2018-2022 Ariadne Conill <ariadne@dereferenced.org>

Portions copyright (c) 2019 Bobby Bingham <koorogi@koorogi.info>

Portions copyright (c) 2019 A. Wilcox <awilfox@adelielinux.org>

Portions copyright (c) 2021 Peng Fan <fanpeng@loongson.cn>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

This software is provided 'as is' and without any warranty, express or
implied.  In no event shall the authors be liable for any damages arising
from the use of this software.

## RSA MD5 License

The MD5 implementation in `lib/cryptLib.c` is derived from the RSA Data Security, Inc. MD5 Message-Digest Algorithm.

Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All rights reserved.

License to copy and use this software is granted provided that it is identified as the "RSA Data Security, Inc. MD5 Message-Digest Algorithm" in all material mentioning or referencing this software or this function. License is also granted to make and use derivative works provided that such works are identified as "derived from the RSA Data Security, Inc. MD5 Message-Digest Algorithm" in all material mentioning or referencing the derived work. RSA Data Security, Inc. makes no representations concerning either the merchantability of this software or the suitability of this software for any particular purpose. It is provided "as is" without express or implied warranty of any kind. These notices must be retained in any copies of any part of this documentation and/or software.

## SHA-1, SHA-256 and Blowfish

The SHA-1 implementation in `lib/cryptLib.c` is derived from the sample code in RFC 3174, Copyright (C) The Internet Society (2001). All Rights Reserved.

The SHA-256 implementation follows the XySSL/PolarSSL `sha2.c` implementation by Christophe Devine.

The Blowfish cipher was designed by Bruce Schneier; the cipher routines follow the layout of Paul Kocher's reference `blowfish.c`. The bcrypt password hashing scheme is by Niels Provos and David Mazieres.

## Mozilla CA Bundle

`certs/roots.crt` is the Mozilla root certificate store as extracted by curl.se (https://curl.se/docs/caextract.html), distributed under the Mozilla Public License 2.0.


## DB, CRYPT, JSON, MQTT, OSDEP, R, URL, UCTX and WEB libraries

- Commercial license, Michael O'Brien

## Trademarks and Copyrights

Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
EmbedThis and Ioto are a registered trademarks of Embedthis Pty Ltd.
Other brands and their products are trademarks of their respective holders.
