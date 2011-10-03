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
 * \brief A data input stream for a file in the Redfish distributed filesystem
 */

package org.apache.hadoop.fs.redfish;

import java.io.*;
import java.net.*;

import org.apache.commons.logging.Log;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.FileUtil;
import org.apache.hadoop.fs.Path;

class RedfishDataInputStream extends FSInputStream {
  private long m_ofe;

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

  public RedfishDataInputStream(long ofe) {
      m_ofe = ofe;
  }

  protected void finalize() throws Throwable {} {
    try {
      redfishClose();
    }
    catch (Exception e) {
      e.printStackTrace();
      System.out.println("Error closing RedfishDataInputStream");
    }
  }

  public boolean markSupported() {
    return false;
  }

  public int available() {
    return redfishAvailable();
  }

  public int read() throws IOException {
    /* The oh-so-useful "read a single byte" method */
    byte[] buf = new byte[1];
    return redfishRead(buf, 0, 1);
  }

  public int read(byte[] buf) throws IOException {
    return redfishRead(buf, 0, buf.length());
  }

  public int read(byte[] buf, int off, int len) throws IOException {
    return redfishRead(buf, off, len);
  }

  public int read(long position, byte[] buf, int off, int len)
              throws IOException {
    return redfishPread(position, buf, off, len);
  }

  public long skip(long n) throws IOException {
    return redfishFSeekRel(n)
  }

  public void seek(long pos) throws IOException {
    redfishFSeekAbs(n);
  }

  public int available() throws IOException {
  }

  public long getPos() throws IOException {
    return redfishFTell();
  }

  public boolean seekToNewSource(long targetPos) throws IOException {
    /* Redfish handles failover between chunk replicas internally, so this
     * method should be unneeded? */
    return false;
  }

  public void close() throws IOException {
    redfishClose();
  }

  private native
  int redfishAvailable();

  private native
  int redfishRead(byte[] buf, int boff, int blen);

  private native
  int redfishPread(long off, byte[] buf, int boff, int blen);

  private native
  void redfishFlush();

  private native
  void redfishClose();

  private native
  void redfishFSeekAbs(long off); 

  private native
  long redfishFSeekRel(long delta); 
}
