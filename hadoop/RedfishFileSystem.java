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
import org.apache.hadoop.fs.BlockLocation;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem.Statistics;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.FileUtil;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.permission.FsPermission;
import org.apache.hadoop.util.Progressable;

public class RedfishFileSystem extends FileSystem {
  private RedfishClient m_client;
  private URI m_uri;
  private String m_cwd;

  private String makeAbsoluteStr(Path path) {
    if (path.isAbsolute()) {
      return path.toUri().getPath();
    }
    else {
      Path p = new Path(this.m_cwd, path);
      return p.toUri().getPath();
    }
  }

  public RedfishFileSystem() {
  }

  @Override
  public URI getUri() {
    return this.m_uri;
  }

  @Override
  public void initialize(URI uri, Configuration conf) throws IOException {
    String configFile = conf.get("fs.redfish.configFile", "");
    if (configFile == "") {
      throw new IOException("You must set fs.redfish.configFile to the " +
          "path to a valid Redfish configuration file");
    }
    String userName = System.getProperty("user.name");
    this.m_client = new RedfishClient(configFile, userName);
    this.m_uri = URI.create(uri.getScheme() + "://" + uri.getAuthority());
    this.m_cwd = new Path("/user", System.getProperty("user.name")).makeQualified(this).toUri().getPath();
    setConf(conf);
  }

  /* This will free most of the resources associated with the RedfishFileSystem.
   */
  @Override
  public void close() throws IOException {
    m_client.redfishDisconnect();
  }

  @Override
  public BlockLocation[] getFileBlockLocations(FileStatus fstatus,
        long start, long len) throws IOException {
    if (fstatus == null) {
      return null;
    }
    if (fstatus.isDir()) {
      return null;
    }
    if ((start<0) || (len < 0)) {
      throw new IllegalArgumentException("Invalid start or len parameter");
    }
    if (fstatus.getLen() < start) {
      return new BlockLocation[0];
    }
    return m_client.redfishGetBlockLocations
              (makeAbsoluteStr(fstatus.getPath()), start, len);
  }

  @Override
  public FSDataInputStream open(Path f, int bufferSize) throws IOException {
    return new FSDataInputStream(m_client.redfishOpen(makeAbsoluteStr(f)));
  }

  @Override
  public FSDataOutputStream create(Path f,
          FsPermission perm,
          boolean overwrite,
          int bufferSize,
          short replication,
          long blockSize,
          Progressable progress) throws IOException
  {
    return new FSDataOutputStream(m_client.redfishCreate(makeAbsoluteStr(f),
                perm.toShort(), bufferSize, replication, blockSize), statistics);
  }

  @Override
  public FSDataOutputStream append(Path f, int bufferSize,
          Progressable progress) throws IOException {
    throw new IOException("not yet implemented");
  }

  @Override
  public boolean rename(Path src, Path dst) throws IOException {
    return m_client.redfishRename(makeAbsoluteStr(src), makeAbsoluteStr(dst));
  }

  @Override
  public boolean delete(Path f) throws IOException {
    return m_client.redfishUnlink(makeAbsoluteStr(f));
  }

  @Override
  public boolean delete(Path f, boolean recursive) throws IOException {
    if (recursive)
      return m_client.redfishUnlinkTree(makeAbsoluteStr(f));
    else
      return m_client.redfishUnlink(makeAbsoluteStr(f));
  }

  @Override
  public FileStatus[] listStatus(Path f) throws IOException {
    return m_client.redfishListDirectory(makeAbsoluteStr(f));
  }

  @Override
  public void setWorkingDirectory(Path wd) {
    this.m_cwd = makeAbsoluteStr(wd);
  }

  @Override
  public Path getWorkingDirectory() {
    return new Path(this.m_cwd);
  }

  @Override
  public boolean mkdirs(Path path, FsPermission perm) throws IOException {
    return m_client.redfishMkdirs(makeAbsoluteStr(path), perm.toShort());
  }

  @Override
  public FileStatus getFileStatus(Path path) throws IOException {
    return m_client.redfishGetPathStatus(makeAbsoluteStr(path));
  }

  @Override
  public void setOwner(Path p, String user, String group) throws IOException {
    m_client.redfishChown(makeAbsoluteStr(p), user, group);
  }

  @Override
  public void setPermission(Path p, FsPermission perm) throws IOException {
    m_client.redfishChmod(makeAbsoluteStr(p), perm.toShort());
  }

  @Override
  public void setTimes(Path p, long mtime, long atime) throws IOException {
    m_client.redfishUtimes(makeAbsoluteStr(p), mtime, atime);
  }
};
