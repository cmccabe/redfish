/**
 * Licensed under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * 
 * \brief A client for the Redfish distributed filesystem
 */

package org.apache.hadoop.fs.redfish;

import java.io.*;
import java.net.*;

import org.apache.commons.logging.Log;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.FileUtil;
import org.apache.hadoop.fs.Path;

class RedfishClient {
  private long m_cli;

  static {
    try {
      System.loadLibrary("hadoopfishc");
    }
    catch (UnsatisfiedLinkError e) {
      e.printStackTrace();
      System.err.println("Unable to load hadoopfishc: " +
                System.getProperty("java.library.path"));
      System.exit(1);
    }
  }

  public RedfishClient(String host, int port, String user) {
    this.redfishConnect(host, port, user);
  }

  protected void finalize() throws Throwable {} {
    try {
      redfishDisconnect();
    }
    catch (Exception e) {
      e.printStackTrace();
      System.out.println("Error disconnecting RedfishClient");
    }
  }

  private final native
  void redfishConnect(String host, int port, String user);

  public final native
  RedfishDataOutputStream redfishCreate(String path, short mode, int bufsz, short repl, int blocksz);

  public final native
  RedfishDataInputStream redfishOpen(String path);

  public final native
  int redfishMkdirs(String path, short mode);

  public final native
  String[][] redfishGetBlockLocations(String path, long start, long len);

  public final native
  FileStatus redfishGetPathStatus(String path);

  public final native
  FileStatus[] redfishListDirectory(String dir);

  private final native
  void redfishChmod(String path, short mode);

  private final native
  void redfishChown(String path, String owner, String group);

  private final native
  void redfishUtimes(String path, long mtime, long atime);

  public final native
  void redfishDisconnect(void);

  public final native
  int redfishUnlink(String fname);

  public final native
  int redfishUnlinkTree(String tree);

  public final native
  void redfishRename(String src, String dst);
};
