swap_in
  takes a page entry struct
  get bitmap index in disk from page entry bitmap index
  blockread on that index
  check that blockread was successful
  change the state of the page to be in memory
  unset the bitmap bit at that index


swap_out
  takes a page entry struct
  find a bitmap index that is not set.
  blockwrite on that index
  check that blockwrite was successful
  change the state of the page to be in disk
  set the bitmap bit at that index
  save the bitmap bit in the page entry
  zero the coremap page


blockread:
  takes a bitmap index, and coremap page index
  KASSERT that the index is flipped
  create the iovec and uio
  VOP_READ()
  get the data from uio->uio_iov->iov_kbase that should have length uio->uio_iov->iov_len
  copy the memory of iov_kbase to the coremap page with memmove
  return 0 for success or -1 for failure


blockwrite:
  takes a bitmap index, and a coremap page index
  create the uio and iovec.
  The void pointer buf will be the coremap page pointer, the length will be PAGE_SIZE
  The segflag will be UIO_KERNELSPACE (double check thats right)
  VOP_WRITE()
  check that all of the data has been written.
  return 0 for success or -1 for failure
