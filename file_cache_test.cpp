#include <fcntl.h>
#include <errno.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>
#include <string>

#include <tr1/random>

#include "buffer.hpp"
#include "callback.hpp"
#include "file_cache.hpp"
#include "logging.hpp"
#include "thread.hpp"
#include "test_unit.hpp"

namespace {

using std::string;
using std::vector;

using base::Buffer;
using base::Callback;
using base::FileCache;
using base::LogMessage;
using base::makeCallableOnce;
using base::makeThread;

// ************************************************************
// Support for creating test files
//
class FileFixture {
public:
  FileFixture(const char** files, size_t files_size)
    : files_(files), files_size_(files_size) {  }
  ~FileFixture() { }

  void startUp();
  void tearDown();

private:
  const char** files_;
  size_t files_size_;

  vector<int> fds_;
  vector<string> names_;

  void createFile(const char* file, size_t size, char c);
  void deleteFiles();

};

void FileFixture::startUp() {
  for (size_t i = 0; i < files_size_; i += 3) {
    size_t size = atoi(files_[i+1]);
    createFile(files_[i], size, *files_[i+2]);
  }
}

void FileFixture::tearDown() {
  for (size_t i = 0; i < fds_.size(); i++) {
    close(fds_[i]);
    unlink(names_[i].c_str());
  }
}

void FileFixture::createFile(const char* name, size_t size, char c) {
  unlink(name);
  int fd = creat(name, S_IRUSR | S_IRGRP);
  if (fd < 0) {
    LOG(LogMessage::ERROR) << "can't create file " << string(name)
                           << ": " << strerror(errno);
    close(fd);
    unlink(name);
    exit(1);
  }

  string content(size, c);
  int bytes = ::write(fd, content.c_str(), size);
  if (bytes < 0) {
    close(fd);
    unlink(name);
    LOG(LogMessage::ERROR) << "can't write to " << string(name)
                           << ": " << *strerror(errno);
    exit(1);
  }

  fds_.push_back(fd);
  names_.push_back(name);
}

// ************************************************************
// Support for concurrent tests
//

class FileTester {
public:
  FileTester(FileCache* c, int num_files);
  ~FileTester() { }

  void start(int requests);
  void join();
  bool error() const { return in_error_; }

private:
  FileCache*     cache_;             // not owned here
  const int      num_files_;
  bool           in_error_;
  pthread_t      tid_;
  vector<string> file_names_;

  void test(int requests);
  int getFileNum(int num) const;

  // Non-copyable, non-assignable
  FileTester(FileTester&);
  FileTester& operator=(FileTester&);

};

FileTester::FileTester(FileCache* c, int num_files)
  : cache_(c), num_files_(num_files), in_error_(false)  {

  // We'll be using an exponential distribution with gamma = 1 (such
  // that 50% of the requests go for the first file, 25%, for the
  // second, and so on). This works well for 5 files.

  for (int i = 1; i <= num_files_; i++) {
    std::ostringstream os;
    os << i << ".html";
    file_names_.push_back(os.str());
  }
}

void FileTester::start(int requests) {
  Callback<void>* body = makeCallableOnce(&FileTester::test, this, requests);
  tid_ = makeThread(body);
}

void FileTester::join() {
  pthread_join(tid_, NULL);
}

void FileTester::test(int requests) {
  int i;
  char c;
  FileCache::CacheHandle h;
  Buffer* buf;

  std::tr1::ranlux64_base_01 eng;
  // std::tr1::exponential_distribution<int> exp_distr;
  std::tr1::uniform_int<int> unif_distr(0, num_files_ - 1);

  while (requests-- > 0) {
    // i = exp_distr(eng) % num_files_;
    i = unif_distr(eng);
    c = char('0' + i+1);
    h = cache_->pin(file_names_[i], &buf, NULL);
    if (h != 0) {
      Buffer::Iterator it = buf->begin();
      while (! it.eob()) {
        if (it.getChar() != c) {
          in_error_ = true;
          break;
        }
        it.next();
      }
      cache_->unpin(h);
    }
  }
}

// ************************************************************
// Test cases
//

TEST(SingleClient, SingleFile) {
  FileCache cache(3000);

  Buffer* buf;
  FileCache::CacheHandle h = cache.pin("a.html", &buf, NULL);
  EXPECT_GT(h, 0);
  EXPECT_EQ(cache.bytesUsed(), 2500);
  EXPECT_EQ(cache.pins(), 1);
  EXPECT_EQ(cache.hits(), 0);
  EXPECT_EQ(cache.failed(), 0);

  string read_string(buf->readPtr(), buf->readSize());
  EXPECT_EQ(string(2500, 'x'), read_string);

  cache.unpin(h);
  EXPECT_EQ(cache.bytesUsed(), 2500);

  h = cache.pin("a.html", &buf, NULL);
  EXPECT_EQ(cache.bytesUsed(), 2500);
  EXPECT_EQ(cache.pins(), 2);
  EXPECT_EQ(cache.hits(), 1);
  EXPECT_EQ(cache.failed(), 0);
}

TEST(SingleClient, Eviction) {
  FileCache cache(3000);

  Buffer* buf;
  FileCache::CacheHandle h = cache.pin("a.html", &buf, NULL);
  cache.unpin(h);
  EXPECT_EQ(cache.bytesUsed(), 2500);

  h = cache.pin("b.html", &buf, NULL);
  EXPECT_GT(h, 0);
  EXPECT_EQ(cache.bytesUsed(), 2500);
  EXPECT_EQ(cache.pins(), 2);
  EXPECT_EQ(cache.hits(), 0);
  EXPECT_EQ(cache.failed(), 0);
}

TEST(SingleClient, MultiplePin) {
  FileCache cache(3000);

  Buffer* buf1;
  FileCache::CacheHandle h1 = cache.pin("a.html", &buf1, NULL);
  Buffer* buf2;
  FileCache::CacheHandle h2 = cache.pin("a.html", &buf2, NULL);
  Buffer* buf3;
  FileCache::CacheHandle h3 = cache.pin("a.html", &buf3, NULL);
  EXPECT_EQ(cache.bytesUsed(), 2500);
  EXPECT_EQ(cache.pins(), 3);
  EXPECT_EQ(cache.hits(), 2);

  cache.unpin(h1);
  cache.unpin(h2);
  cache.unpin(h3);
  EXPECT_EQ(cache.bytesUsed(), 2500);
}

TEST(SingleClient, NoUnpinnedSpaceEnough) {
  FileCache cache(3000);

  Buffer* buf_a;
  EXPECT_GT(cache.pin("a.html", &buf_a, NULL), 0);
  EXPECT_GT(buf_a, 0);

  Buffer* buf_b;
  EXPECT_EQ(cache.pin("b.html", &buf_b, NULL), 0);
  EXPECT_EQ(buf_b, 0);
}

TEST(SingleClient, FileTooBig) {
  FileCache cache(3000);

  Buffer* buf_a = 0;
  EXPECT_EQ(cache.pin("c.html", &buf_a, NULL), 0);
  EXPECT_EQ(buf_a, 0);

  EXPECT_EQ(cache.pins(), 1);
  EXPECT_EQ(cache.failed(), 1);
}

TEST(Failure, NegativeReferenceCount) {
  FileCache cache(3000);

  Buffer* buf;
  FileCache::CacheHandle h = cache.pin("a.html", &buf, NULL);
  cache.unpin(h);
  EXPECT_FATAL(cache.unpin(h));
}

TEST(Concurrency, Mayhem) {
  const int num_files = 5;
  FileCache cache(2048 * (num_files - 2)); // not enough space for all files

  const int threads = 4;
  const int requests = 100;
  FileTester* testers[threads];

  for (int i = 0; i < threads; i++) {
    testers[i] = new FileTester(&cache, 5 /* files 1-5.html */);
  }
  for (int i = 0; i < threads; i++) {
    testers[i]->start(requests);
  }
  for (int i = 0; i < threads; i++) {
    testers[i]->join();
    EXPECT_FALSE(testers[i]->error());
    delete testers[i];
  }

  std::cout << "hits/missed/failed "
            << cache.hits() << "/"
            << cache.pins() - cache.failed() - cache.hits() << "/"
            << cache.failed() <<  " " ;

  EXPECT_EQ(cache.pins(), threads*requests);
}

}  // unnamed namespace

int main(int argc, char *argv[]) {
  const char* files[] =  { "a.html", "2500", "x",
                           "b.html", "2500", "y",
                           "c.html", "5000", "z",
                           "1.html", "2048", "1",
                           "2.html", "2048", "2",
                           "3.html", "2048", "3",
                           "4.html", "2048", "4",
                           "5.html", "2048", "5"  };
  size_t size = sizeof(files)/sizeof(files[0]);
  FileFixture ff(files, size);
  ff.startUp();

  int res = RUN_TESTS(argc, argv);

  ff.tearDown();
  return res;
}
