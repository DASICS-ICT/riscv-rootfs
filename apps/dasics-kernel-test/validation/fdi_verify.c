// SPDX-License-Identifier: GPL-2.0-only
#include <linux/atomic.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/user_namespace.h>
#include <asm/dasics_verify.h>

#ifndef CONFIG_RISCV_DASICS_VALIDATION
#error "The validation module requires CONFIG_RISCV_DASICS_VALIDATION"
#endif

struct fdi_verify_file {
	struct mutex lock;
	struct task_struct *owner;
	struct mm_struct *mm;
	u64 cookie;
	struct riscv_dasics_revoke_work *revocation;
};

/* Exclusivity follows the file lifetime, which can cross task boundaries. */
static atomic_t opened = ATOMIC_INIT(0);
static atomic64_t cookies = ATOMIC64_INIT(0);

static int verify_open(struct inode *inode, struct file *file)
{
	struct fdi_verify_file *context;

	if (!current->mm || !ns_capable(&init_user_ns, CAP_SYS_RAWIO))
		return -EPERM;
	if (atomic_cmpxchg(&opened, 0, 1))
		return -EBUSY;
	context = kzalloc(sizeof(*context), GFP_KERNEL);
	if (!context) {
		atomic_set(&opened, 0);
		return -ENOMEM;
	}
	mutex_init(&context->lock);
	context->owner = current;
	get_task_struct(current);
	context->mm = current->mm;
	mmgrab(context->mm);
	file->private_data = context;
	return nonseekable_open(inode, file);
}

static long verify_ioctl(struct file *file, unsigned int command, unsigned long argument)
{
	struct fdi_verify_file *context = file->private_data;
	struct fdi_verify_result request, result;
	void __user *user = (void __user *)argument;
	int error;

	if (current != context->owner || current->mm != context->mm ||
	    !ns_capable(&init_user_ns, CAP_SYS_RAWIO))
		return -EPERM;
	if (command != FDI_VERIFY_BEGIN && command != FDI_VERIFY_ARM &&
	    command != FDI_VERIFY_QUERY && command != FDI_VERIFY_END)
		return -ENOTTY;
	if (copy_from_user(&request, user, sizeof(request)))
		return -EFAULT;
	if (request.version != FDI_VERIFY_VERSION)
		return -EINVAL;
	mutex_lock(&context->lock);
	if (command == FDI_VERIFY_BEGIN) {
		u64 cookie = atomic64_inc_return(&cookies);

		if (context->cookie) {
			error = -EBUSY;
			goto out;
		}
		if (!context->revocation) {
			context->revocation = riscv_dasics_verify_alloc_revoke(current);
			if (!context->revocation) {
				error = -ENOMEM;
				goto out;
			}
		}
		error = riscv_dasics_verify_begin(cookie, request.flags);
		if (error)
			goto out;
		context->cookie = cookie;
	} else if (!context->cookie) {
		error = -ESTALE;
		goto out;
	}
	if (command == FDI_VERIFY_ARM) {
		error = riscv_dasics_verify_arm(context->cookie, request.kind, request.site);
		if (error)
			goto out;
	}
	if (command == FDI_VERIFY_END) {
		error = riscv_dasics_verify_end(context->cookie, &result);
		context->cookie = 0;
	} else {
		error = riscv_dasics_verify_query(context->cookie, &result);
	}
	if (!error && copy_to_user(user, &result, sizeof(result)))
		error = -EFAULT;
	if (error && command == FDI_VERIFY_BEGIN) {
		riscv_dasics_verify_revoke(current, context->cookie, &context->revocation);
		context->cookie = 0;
	}
out:
	mutex_unlock(&context->lock);
	return error;
}

static int revoke_locked(struct fdi_verify_file *context)
{
	int error = riscv_dasics_verify_revoke(context->owner, context->cookie, &context->revocation);

	if (!error)
		context->cookie = 0;
	else
		pr_err("FDI_VERIFY_REVOKE result=FAIL pid=%d error=%d\n",
		       task_pid_vnr(context->owner), error);
	return error;
}

static int verify_flush(struct file *file, fl_owner_t owner)
{
	struct fdi_verify_file *context = file->private_data;
	int error = 0;

	mutex_lock(&context->lock);
	/* Any alias close terminates this test window. This avoids races with
	 * file-table unsharing; revocation still runs only in the owning task. */
	error = revoke_locked(context);
	mutex_unlock(&context->lock);
	return error;
}

static int verify_release(struct inode *inode, struct file *file)
{
	struct fdi_verify_file *context = file->private_data;

	mutex_lock(&context->lock);
	revoke_locked(context);
	riscv_dasics_verify_free_revoke(context->revocation);
	mutex_unlock(&context->lock);
	mmdrop(context->mm);
	put_task_struct(context->owner);
	kfree(context);
	atomic_set(&opened, 0);
	return 0;
}

static const struct file_operations verify_operations = {
	.owner = THIS_MODULE,
	.open = verify_open,
	.unlocked_ioctl = verify_ioctl,
	.flush = verify_flush,
	.release = verify_release,
};

static struct miscdevice verify_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "fdi-verify",
	.fops = &verify_operations,
	.mode = 0600,
};

static int __init verify_init(void)
{
	return misc_register(&verify_device);
}

static void __exit verify_exit(void)
{
	misc_deregister(&verify_device);
}

module_init(verify_init);
module_exit(verify_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Restricted FDI software-test observation and CBO environment");
