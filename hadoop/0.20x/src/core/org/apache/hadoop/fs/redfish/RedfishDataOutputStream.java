/*
 * Copyright 2011-2012 the RedFish authors
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
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.FileUtil;
import org.apache.hadoop.fs.Path;

class RedfishDataOutputStream extends OutputStream {
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

  public RedfishDataOutputStream(long ofe) {
      m_ofe = ofe;
  }

  protected void finalize() throws Throwable {} {
    try {
      redfishClose();
    }
    catch (Exception e) {
      e.printStackTrace();
      System.out.println("Error closing RedfishDataOutputStream");
    }
  }

  public void write(int b) throws IOException {
    byte[] buf = new byte[1];
    buf[0] = b;
    redfishWrite(buf);
  }

  public void write(byte[] b) throws IOException {
    redfishWrite(b);
  }

//  public void write(byte[] b, int off, int len) throws IOException {
//  }

  public void flush() throws IOException {
    this.redfishFlush();
  }

  public void close() throws IOException {
    this.redfishClose();
  }

  private native
  int redfishWrite(byte[] buf);

  private native
  void redfishFlush();

  private native
  void redfishClose();
}
