/*
 * Copyright 2011-2012 the Redfish authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.hadoop.fs.redfish;

import java.io.*;
import java.net.*;

import org.apache.commons.logging.Log;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.FileUtil;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.PositionedReadable;
import org.apache.hadoop.fs.Seekable;

class RedfishDataInputStream extends InputStream
            implements Seekable, PositionedReadable, Closeable {
  private long m_ofe;

  static {
    try {
      System.loadLibrary("hfishc");
    }
    catch (UnsatisfiedLinkError e) {
      e.printStackTrace();
      System.err.println("Unable to load hfishc: " +
                System.getProperty("java.library.path"));
      System.exit(1);
    }
  }

  private RedfishDataInputStream(long ofe) {
      m_ofe = ofe;
  }

  /* This finalizer is intended to free the (small!) amount of memory used by
   * the redfish_file data structure of a closed file.  It also destroys the
   * pthread mutex used to make the rest of the functions thread-safe.
   *
   * Please call close() explicitly to close files.  This it NOT intended as a
   * replacement for that function.
   * */
  protected void finalize() throws Throwable {} {
    this.redfishFree();
  }

  @Override
  public boolean markSupported() {
    return false;
  }

  @Override
  public int read() throws IOException {
    int amt;

    /* The oh-so-useful "read a single byte" method */
    byte[] buf = new byte[1];
    amt = redfishRead(buf, 0, 1);
    if (amt != 1)
      return -1;
    return buf[0];
  }

  @Override
  public int read(byte[] buf) throws IOException {
    return redfishRead(buf, 0, buf.length);
  }

  @Override
  public int read(byte[] buf, int off, int len) throws IOException {
    return redfishRead(buf, off, len);
  }

  @Override
  public int read(long pos, byte[] buf, int off, int len) throws IOException {
    return redfishPread(pos, buf, off, len);
  }

  @Override
  public void readFully(long pos, byte[] buf, int off, int len) throws IOException {
    int nread = redfishPread(pos, buf, off, len);
    if (nread < len) {
      throw new EOFException("End of file reached before reading fully.");
    }
  }

  @Override
  public void readFully(long pos, byte[] buf) throws IOException {
    readFully(pos, buf, 0, buf.length);
  }

  @Override
  public boolean seekToNewSource(long targetPos) throws IOException {
    /* Redfish handles failover between chunk replicas internally, so this
     * method should be unneeded? */
    return false;
  }

  @Override
  public native
    int available() throws IOException;

  private native
    int redfishRead(byte[] buf, int boff, int blen) throws IOException;

  private native
    int redfishPread(long off, byte[] buf, int boff, int blen) throws IOException;

  @Override
  public native
    void close() throws IOException;

  private native
    void free();

  @Override
  public native
    void seek(long off) throws IOException;

  @Override
  public native
    long getPos() throws IOException;

  private native
    void redfishFree();
}
