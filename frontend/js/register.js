document.getElementById('registerForm').addEventListener('submit', async (e) => {
    e.preventDefault();

    // 获取表单数据
    const formData = {
        name: document.getElementById('name').value,
        idCard: document.getElementById('idCard').value,
        phone: document.getElementById('phone').value,
        address: document.getElementById('address').value,
        cardNumber: document.getElementById('cardNumber').value,
        password: document.getElementById('password').value,
        confirmPassword: document.getElementById('confirmPassword').value,
        initialDeposit: parseFloat(document.getElementById('initialDeposit').value)
    };

    // 表单验证
    if (!validateForm(formData)) {
        return;
    }

    try {
        const response = await fetch('/api/register', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(formData)
        });

        const data = await response.text();

        if (response.ok) {
            alert('注册成功！');
            window.location.href = 'login.html';
        } else {
            alert('注册失败：' + data);
        }
    } catch (error) {
        alert('注册失败：' + error.message);
    }
});

function validateForm(formData) {
    // 验证密码匹配
    if (formData.password !== formData.confirmPassword) {
        alert('两次输入的密码不匹配！');
        return false;
    }

    // 验证身份证号格式
    if (!/^\d{17}[\dXx]$/.test(formData.idCard)) {
        alert('请输入有效的身份证号！');
        return false;
    }

    // 验证手机号格式
    if (!/^1[3-9]\d{9}$/.test(formData.phone)) {
        alert('请输入有效的手机号！');
        return false;
    }

    // 验证银行卡号格式（假设为16-19位数字）
    if (!/^\d{16,19}$/.test(formData.cardNumber)) {
        alert('请输入有效的银行卡号！');
        return false;
    }

    // 验证初始存款金额
    if (formData.initialDeposit < 0) {
        alert('初始存款金额不能为负数！');
        return false;
    }

    return true;
}
