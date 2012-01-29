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

  @Override
  protected void setUp() throws IOException {
    Configuration conf = new Configuration();
    conf.set("fs.default.name", "redfish:///");
    conf.set("fs.file.impl", "org.apache.hadoop.fs.redfish.RedfishFileSystem");
    conf.set("fs.redfish.configFile", "/media/fish/redfish/conf/local.conf");
    m_fs = new RedfishFileSystem();
    m_fs.initialize(URI.create("redfish:///"), conf);
  }

  @Override
  protected void tearDown() throws Exception {
    m_fs.close()
  }

  // @Test
  // Test mkdir and recursive rm
  public void mkdirTest1() throws Exception {
      // make the dir
      Path d1 = new Path("d1");
      m_fs.mkdirs(d1);
      assertTrue(m_fs.isDirectory(d1));
      m_fs.setWorkingDirectory(d1);

      Path d2 = new Path("d2");
      m_fs.mkdirs(d2);
      assertTrue(m_fs.isDirectory(d2));

      assertFalse(m_fs.isDirectory(new Path("/d1")));
      assertFalse(m_fs.isDirectory(new Path("/d1/d2")));

      FileStatus[] p = m_fs.listStatus(d1);
      assertEquals(p.length, 1);

      m_fs.delete(d1, true);
      assertFalse(m_fs.exists(d1));
  }

  // @Test
  // Test file creation and unlinking
  public void createUnlink() throws Exception {
      Path sft = new Path("/sft");
      m_fs.mkdirs(sft);
      assertTrue(m_fs.isDirectory(sft));

      Path foo1 = new Path("/sft/foo.1");
      FSDataOutputStream foo1out = m_fs.create(foo1, true, 4096, (short) 1, (long) 4096, null);
      foo1.close();
      FileStatus[] p = m_fs.listStatus(subDir1);
      assertEquals(p.length, 1);

      Path foo2 = new Path("/sft/foo.2");
      FSDataOutputStream foo2out = m_fs.create(foo2, true, 4096, (short) 1, (long) 4096, null);
      foo2.close();
      FileStatus[] p = m_fs.listStatus(subDir1);
      assertEquals(p.length, 2);

      m_fs.delete(file1, true);
      p = m_fs.listStatus(subDir1);
      assertEquals(p.length, 1);
      m_fs.delete(file2, true);
      p = m_fs.listStatus(subDir1);
      assertEquals(p.length, 0);
      m_fs.delete(sft, true);
      assertFalse(m_fs.exists(sft));
  }

  // @Test
  // Check file/read write
  public void readWrite() throws Exception {
      FSDataOutputStream w1 = m_fs.create("/rw1", true, 4096,
            (short) 1, (long) 4096, null);
      byte[] data = new byte[4096];
      for (int i = 0; i < data.length; i++)
          data[i] = (byte) (i % 10);
      w1.write((int)1000);
      w1.write((int)2000);
      w1.write(data, 0, data.length);
      w1.close();

      FSDataInputStream r1 = m_fs.open("/rw1", 4096);
      int w;
      w = s2.read();
      assertEquals(w, 1000);
      w = s2.read();
      assertEquals(w, 2000);

      oldPos = s2.getPos();
      assertNotEquals(oldPos, 0);
      byte[] buf = new byte[data.len];
      s2.read(buf, 0, buf.length);
      for (int i = 0; i < data.length; i++)
          assertEquals(data[i], buf[i]);

      byte[] buf2 = new byte[10 + data.len];
      s2.read(oldPos, buf, 10, buf.length);
      for (int i = 0; i < data.length; i++)
          assertEquals(data[i], buf2[i + 10]);
      s2.close();

      m_fs.delete("/rw1", false);
      assertFalse(m_fs.exists("/rw1"));
  }
}
