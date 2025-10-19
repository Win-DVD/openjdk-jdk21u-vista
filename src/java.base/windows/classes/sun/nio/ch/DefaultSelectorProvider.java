/*
 * Copyright (c) 2001, 2021, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package sun.nio.ch;

import java.security.AccessController;
import java.security.PrivilegedAction;

/**
 * Creates this platform's default SelectorProvider
 */
@SuppressWarnings("removal")
public class DefaultSelectorProvider {
    private static final SelectorProviderImpl INSTANCE;

    static {
        PrivilegedAction<SelectorProviderImpl> pa = () -> {
            // optional kill switch
            boolean disableWEPoll = Boolean.getBoolean("jdk.nio.disableWEPoll");

            // only try WEPoll on win7+ and if not disabled
            if (!disableWEPoll && isWin7OrLater()) {
                try {
                    // we fall through to WindowsSelectorProvider.
                    return new WEPollSelectorProvider();
                } catch (Throwable t) {
                    // any failure with WEPoll we use the legacy provider
                }
            }
            // vista (6.0) or any failure we go to legacy selector
            return new WindowsSelectorProvider();
        };
        INSTANCE = AccessController.doPrivileged(pa);
    }

    private DefaultSelectorProvider() { }

    public static SelectorProviderImpl get() {
        return INSTANCE;
    }

    // vista == 6.0, windows 7 == 6.1. allow WEPoll only on 6.1+
    private static boolean isWin7OrLater() {
        String v = AccessController.doPrivileged(
                (PrivilegedAction<String>) () -> System.getProperty("os.version", ""));
        if (v == null || v.isEmpty()) return false;

        int major = 0, minor = 0;
        try {
            String[] parts = v.split("\\.");
            if (parts.length > 0) major = Integer.parseInt(parts[0]);
            if (parts.length > 1) minor = Integer.parseInt(parts[1]);
        } catch (NumberFormatException ignored) {
            // if parsing fails do not enable WEPoll
            return false;
        }
        if (major > 6) return true;
        if (major == 6) return minor >= 1; // 6.1 = win7+
        return false;
    }
}
