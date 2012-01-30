/*
 * Copyright 2012 the Redfish authors
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
import junit.framework.TestCase;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.FileUtil;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.redfish.RedfishFileSystem;

public class TestRedfishFileSystem extends TestCase {
  private RedfishFileSystem m_fs;
  public static final Log LOG = LogFactory.getLog(
      "org.apache.hadoop.fs.redfish.TestRedfishFileSystem");
  private final Path m_testBase = new Path("tbase");

  @Override
  protected void setUp() throws IOException {
    Configuration conf = new Configuration();
    conf.set("fs.default.name", "redfish:///");
    conf.set("fs.file.impl", "org.apache.hadoop.fs.redfish.RedfishFileSystem");
    conf.set("fs.redfish.configFile", "/media/fish/redfish/conf/local.conf");
    m_fs = new RedfishFileSystem();
    m_fs.initialize(URI.create("redfish:///"), conf);
    m_fs.delete(m_testBase, true);
    m_fs.mkdirs(m_testBase);
  }

  @Override
  protected void tearDown() throws Exception {
    m_fs.close();
  }

  // @Test
  // Test mkdir and recursive rm
  public void testMkdir() throws Exception {
    // make the dir
    Path d1 = new Path(m_testBase, "d1");
    m_fs.mkdirs(d1);
    assertTrue(m_fs.isDirectory(d1));
    m_fs.setWorkingDirectory(d1);

    Path d2 = new Path("d2");
    m_fs.mkdirs(d2);
    assertTrue(m_fs.isDirectory(d2));
    m_fs.setWorkingDirectory(new Path(".."));
    assertTrue(m_fs.isDirectory(new Path("d1/d2")));
    assertFalse(m_fs.isDirectory(new Path("d1/nonexistent")));

    FileStatus[] p = m_fs.listStatus(new Path("."));
    assertEquals(1, p.length);

    m_fs.delete(d1, true);
    assertFalse(m_fs.exists(d1));
  }

  // @Test
  // Test file creation and unlinking
  public void testUnlink() throws Exception {
    Path sft = new Path(m_testBase, "sft");
    m_fs.mkdirs(sft);
    assertTrue(m_fs.isDirectory(sft));

    Path foo1 = new Path(sft, "foo.1");
    FSDataOutputStream foo1out = m_fs.create(foo1, true, 4096, (short) 1, (long) 4096, null);
    assertNotSame(foo1out, null);
    foo1out.close();
    FileStatus[] p = m_fs.listStatus(sft);
    assertNotSame(null, p);
    assertEquals(1, p.length);

    Path foo2 = new Path(sft, "foo.2");
    FSDataOutputStream foo2out = m_fs.create(foo2, true, 4096, (short) 1, (long) 4096, null);
    assertNotSame(null, foo2out);
    foo2out.close();
    p = m_fs.listStatus(sft);
    assertNotSame(null, p);
    assertEquals(2, p.length);

    m_fs.delete(foo1, true);
    p = m_fs.listStatus(sft);
    assertNotSame(null, p);
    assertEquals(1, p.length);
    m_fs.delete(foo2, true);
    p = m_fs.listStatus(sft);
    assertNotSame(null, p);
    assertEquals(0, p.length);
    m_fs.delete(sft, true);
    assertFalse(m_fs.exists(sft));
  }

  // @Test
  // Check file/read write
  public void testReadAndWrite() throws Exception {
    Path rwPath = new Path(m_testBase, "rw1");
    FSDataOutputStream w1 = m_fs.create(rwPath, true, 4096,
          (short) 1, (long) 4096, null);
    byte[] data = new byte[4096];
    for (int i = 0; i < data.length; i++)
        data[i] = (byte) (i % 10);
    w1.write((int)250);
    w1.write((int)123);
    w1.write(data, 0, data.length);
    w1.close();

    FSDataInputStream r1 = m_fs.open(rwPath, 4096);
    int w;
    w = r1.read();
    assertEquals(250, w);
    w = r1.read();
    assertEquals(123, w);

    long oldPos = r1.getPos();
    assertTrue(oldPos != 0);
    byte[] buf = new byte[data.length];
    r1.read(buf, 0, buf.length);
    for (int i = 0; i < data.length; i++)
        assertEquals(data[i], buf[i]);

    final int TEST_OFF = 10;
    byte[] buf2 = new byte[TEST_OFF + data.length];

    /* test out-of-bounds handling */
    boolean gotException = false;
    try {
      r1.read(oldPos, buf, TEST_OFF, data.length);
    }
    catch (java.lang.IndexOutOfBoundsException e) {
      gotException = true;
    }
    assertTrue(gotException);

    /* test pread */
    r1.read(oldPos, buf2, TEST_OFF, data.length);
    for (int i = 0; i < data.length; i++)
        assertEquals(data[i], buf2[i + TEST_OFF]);
    r1.close();

    m_fs.delete(rwPath, false);
    assertFalse(m_fs.exists(rwPath));
  }
}
